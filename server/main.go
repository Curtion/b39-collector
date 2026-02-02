package main

import (
	"embed"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"log"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/glebarez/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

//go:embed all:web/dist
var webDist embed.FS

// SensorData 传感器数据模型 (B39检测仪)
type SensorData struct {
	ID          uint      `gorm:"column:id;primaryKey;autoIncrement;comment:主键ID" json:"id"`
	CreatedAt   time.Time `gorm:"column:created_at;index;comment:数据接收时间" json:"created_at"`
	Particle    float64   `gorm:"column:particle;comment:>0.3um颗粒数(pcs/0.1L)" json:"particle"`             // V1: >0.3um颗粒数
	PM25        float64   `gorm:"column:pm25;comment:PM2.5(μg/m³)" json:"pm25"`                            // V2: PM2.5
	HCHO        float64   `gorm:"column:hcho;comment:甲醛(μg/m³)" json:"hcho"`                               // V3: 甲醛
	CO2         float64   `gorm:"column:co2;comment:CO2(PPM)" json:"co2"`                                  // V4: CO2
	Temperature float64   `gorm:"column:temperature;comment:温度(℃)" json:"temperature"`                     // V5: 温度
	Humidity    float64   `gorm:"column:humidity;comment:湿度(%)" json:"humidity"`                           // V6: 湿度
	VOC         float64   `gorm:"column:voc;comment:VOC(ppb)" json:"voc"`                                  // V7: VOC
	SequenceNum int64     `gorm:"column:sequence_num;index;comment:设备递增序号(用于判断传感器状态)" json:"sequence_num"` // V8: 序号
	IsValid     bool      `gorm:"column:is_valid;index;comment:传感器是否正常(true=序号递增正常,false=可能故障)" json:"is_valid"`
}

var (
	db            *gorm.DB
	lastSequence  int64
	sequenceMutex sync.Mutex
)

func main() {
	if err := initDB(); err != nil {
		log.Fatal("数据库初始化失败:", err)
	}

	distFS, err := fs.Sub(webDist, "web/dist")
	if err != nil {
		log.Fatal("静态资源加载失败:", err)
	}

	http.HandleFunc("/api/data", handleData)
	http.HandleFunc("/api/status", handleStatus)
	http.HandleFunc("/api/history", handleHistory)
	http.Handle("/", http.FileServer(http.FS(distFS)))

	log.Println("服务已启动:8080")
	log.Fatal(http.ListenAndServe(":8080", nil))
}

// initDB 初始化数据库
func initDB() error {
	var err error
	db, err = gorm.Open(sqlite.Open("sensor.db"), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		return err
	}

	// 自动迁移表结构
	if err := db.AutoMigrate(&SensorData{}); err != nil {
		return err
	}

	// 从数据库加载最后的序列号
	var latest SensorData
	if err := db.Order("sequence_num desc").First(&latest).Error; err == nil {
		lastSequence = latest.SequenceNum
		log.Printf("加载的最后序列号: %d\n", lastSequence)
	}

	return nil
}

func handleData(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "请求方法不允许", http.StatusMethodNotAllowed)
		return
	}

	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "读取请求体失败", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	var req struct {
		Data string `json:"data"`
	}
	if err := json.Unmarshal(body, &req); err != nil {
		http.Error(w, "JSON格式错误", http.StatusBadRequest)
		return
	}

	// 解析逗号分隔的数据
	fields := strings.Split(req.Data, ",")
	if len(fields) != 8 {
		http.Error(w, "数据格式错误, 需要8个字段", http.StatusBadRequest)
		return
	}

	// 转换数据
	values := make([]float64, 8)
	for i, f := range fields {
		v, err := strconv.ParseFloat(strings.TrimSpace(f), 64)
		if err != nil {
			http.Error(w, fmt.Sprintf("第%d个字段数值错误", i+1), http.StatusBadRequest)
			return
		}
		values[i] = v
	}

	sequenceNum := int64(values[7])

	// 检查传感器状态（序号是否递增）
	sequenceMutex.Lock()
	isValid := sequenceNum > lastSequence
	if isValid {
		lastSequence = sequenceNum
	}
	sequenceMutex.Unlock()

	// 保存到数据库
	sensorData := SensorData{
		Particle:    values[0], // V1: >0.3um颗粒数
		PM25:        values[1], // V2: PM2.5
		HCHO:        values[2], // V3: 甲醛
		CO2:         values[3], // V4: CO2
		Temperature: values[4], // V5: 温度
		Humidity:    values[5], // V6: 湿度
		VOC:         values[6], // V7: VOC
		SequenceNum: sequenceNum,
		IsValid:     isValid,
	}

	if err := db.Create(&sensorData).Error; err != nil {
		http.Error(w, "保存数据失败", http.StatusInternalServerError)
		return
	}

	// 打印接收到的数据
	prettyJSON, _ := json.MarshalIndent(sensorData, "", "  ")
	fmt.Printf("收到数据:\n%s\n", string(prettyJSON))

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]any{
		"status":  "success",
		"data":    sensorData,
		"message": map[bool]string{true: "传感器工作正常", false: "传感器可能存在问题"}[isValid],
	})
}

// handleStatus 获取传感器当前状态
func handleStatus(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "请求方法不允许", http.StatusMethodNotAllowed)
		return
	}

	var latest SensorData
	if err := db.Order("created_at desc").First(&latest).Error; err != nil {
		http.Error(w, "暂无数据", http.StatusNotFound)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{
		"sensor_status": map[bool]string{true: "正常", false: "异常"}[latest.IsValid],
		"last_sequence": latest.SequenceNum,
		"last_data":     latest,
	})
}

// handleHistory 获取历史数据
func handleHistory(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "请求方法不允许", http.StatusMethodNotAllowed)
		return
	}

	limitStr := r.URL.Query().Get("limit")
	limit := 100
	if limitStr != "" {
		if l, err := strconv.Atoi(limitStr); err == nil && l > 0 {
			limit = l
		}
	}

	var records []SensorData
	if err := db.Order("created_at desc").Limit(limit).Find(&records).Error; err != nil {
		http.Error(w, "获取数据失败", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{
		"count": len(records),
		"data":  records,
	})
}
