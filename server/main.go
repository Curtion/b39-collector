package main

import (
	"embed"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"log"
	"math"
	"net/http"
	"sort"
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
	http.HandleFunc("/api/stats", handleStats)
	http.HandleFunc("/api/analysis", handleAnalysis)
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

	// 支持 hours 参数按时间范围查询
	hoursStr := r.URL.Query().Get("hours")
	limitStr := r.URL.Query().Get("limit")

	var records []SensorData
	query := db.Order("created_at desc")

	if hoursStr != "" {
		if h, err := strconv.Atoi(hoursStr); err == nil && h > 0 {
			startTime := time.Now().Add(-time.Duration(h) * time.Hour)
			query = query.Where("created_at >= ?", startTime)
		}
	}

	if limitStr != "" {
		if l, err := strconv.Atoi(limitStr); err == nil && l > 0 {
			query = query.Limit(l)
		}
	}

	if err := query.Find(&records).Error; err != nil {
		http.Error(w, "获取数据失败", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{
		"count": len(records),
		"data":  records,
	})
}

// StatsResult 统计结果
type StatsResult struct {
	Min    float64 `json:"min"`
	Max    float64 `json:"max"`
	Avg    float64 `json:"avg"`
	Median float64 `json:"median"`
	StdDev float64 `json:"std_dev"`
	Count  int     `json:"count"`
}

// 计算统计数据
func calculateStats(values []float64) StatsResult {
	if len(values) == 0 {
		return StatsResult{}
	}

	// 排序用于计算中位数
	sorted := make([]float64, len(values))
	copy(sorted, values)
	sort.Float64s(sorted)

	// 计算各项统计值
	min, max := sorted[0], sorted[len(sorted)-1]
	sum := 0.0
	for _, v := range values {
		sum += v
	}
	avg := sum / float64(len(values))

	// 中位数
	median := sorted[len(sorted)/2]
	if len(sorted)%2 == 0 {
		median = (sorted[len(sorted)/2-1] + sorted[len(sorted)/2]) / 2
	}

	// 标准差
	variance := 0.0
	for _, v := range values {
		variance += (v - avg) * (v - avg)
	}
	stdDev := math.Sqrt(variance / float64(len(values)))

	return StatsResult{
		Min:    math.Round(min*100) / 100,
		Max:    math.Round(max*100) / 100,
		Avg:    math.Round(avg*100) / 100,
		Median: math.Round(median*100) / 100,
		StdDev: math.Round(stdDev*100) / 100,
		Count:  len(values),
	}
}

// 计算相关系数
func calculateCorrelation(x, y []float64) float64 {
	if len(x) != len(y) || len(x) == 0 {
		return 0
	}

	n := float64(len(x))
	sumX, sumY, sumXY, sumX2, sumY2 := 0.0, 0.0, 0.0, 0.0, 0.0

	for i := range x {
		sumX += x[i]
		sumY += y[i]
		sumXY += x[i] * y[i]
		sumX2 += x[i] * x[i]
		sumY2 += y[i] * y[i]
	}

	numerator := n*sumXY - sumX*sumY
	denominator := math.Sqrt((n*sumX2 - sumX*sumX) * (n*sumY2 - sumY*sumY))

	if denominator == 0 {
		return 0
	}

	return math.Round(numerator/denominator*1000) / 1000
}

// handleStats 获取统计数据
func handleStats(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "请求方法不允许", http.StatusMethodNotAllowed)
		return
	}

	// 获取时间范围参数
	hours := 24
	if h := r.URL.Query().Get("hours"); h != "" {
		if parsed, err := strconv.Atoi(h); err == nil && parsed > 0 {
			hours = parsed
		}
	}

	startTime := time.Now().Add(-time.Duration(hours) * time.Hour)

	var records []SensorData
	if err := db.Where("created_at >= ?", startTime).Order("created_at asc").Find(&records).Error; err != nil {
		http.Error(w, "获取数据失败", http.StatusInternalServerError)
		return
	}

	if len(records) == 0 {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]any{
			"message": "暂无数据",
			"hours":   hours,
		})
		return
	}

	// 提取各项数据
	particles, pm25s, hchos, co2s := make([]float64, len(records)), make([]float64, len(records)), make([]float64, len(records)), make([]float64, len(records))
	temps, humidities, vocs := make([]float64, len(records)), make([]float64, len(records)), make([]float64, len(records))

	for i, r := range records {
		particles[i] = r.Particle
		pm25s[i] = r.PM25
		hchos[i] = r.HCHO
		co2s[i] = r.CO2
		temps[i] = r.Temperature
		humidities[i] = r.Humidity
		vocs[i] = r.VOC
	}

	// 异常检测：检查是否超过阈值
	anomalies := []map[string]any{}
	latest := records[len(records)-1]

	if latest.PM25 > 75 {
		anomalies = append(anomalies, map[string]any{"type": "pm25", "value": latest.PM25, "threshold": 75, "level": "warning", "message": "PM2.5 超标"})
	}
	if latest.PM25 > 150 {
		anomalies = append(anomalies, map[string]any{"type": "pm25", "value": latest.PM25, "threshold": 150, "level": "danger", "message": "PM2.5 严重超标"})
	}
	if latest.CO2 > 1000 {
		anomalies = append(anomalies, map[string]any{"type": "co2", "value": latest.CO2, "threshold": 1000, "level": "warning", "message": "CO2 偏高，建议通风"})
	}
	if latest.CO2 > 2000 {
		anomalies = append(anomalies, map[string]any{"type": "co2", "value": latest.CO2, "threshold": 2000, "level": "danger", "message": "CO2 严重超标"})
	}
	if latest.HCHO > 80 {
		anomalies = append(anomalies, map[string]any{"type": "hcho", "value": latest.HCHO, "threshold": 80, "level": "warning", "message": "甲醛偏高"})
	}
	if latest.HCHO > 100 {
		anomalies = append(anomalies, map[string]any{"type": "hcho", "value": latest.HCHO, "threshold": 100, "level": "danger", "message": "甲醛超标"})
	}
	if latest.VOC > 500 {
		anomalies = append(anomalies, map[string]any{"type": "voc", "value": latest.VOC, "threshold": 500, "level": "warning", "message": "VOC 偏高"})
	}
	if !latest.IsValid {
		anomalies = append(anomalies, map[string]any{"type": "sensor", "level": "danger", "message": "传感器可能故障"})
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{
		"hours":      hours,
		"count":      len(records),
		"start_time": startTime,
		"end_time":   time.Now(),
		"stats": map[string]StatsResult{
			"particle":    calculateStats(particles),
			"pm25":        calculateStats(pm25s),
			"hcho":        calculateStats(hchos),
			"co2":         calculateStats(co2s),
			"temperature": calculateStats(temps),
			"humidity":    calculateStats(humidities),
			"voc":         calculateStats(vocs),
		},
		"anomalies": anomalies,
	})
}

// handleAnalysis 获取分析数据
func handleAnalysis(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "请求方法不允许", http.StatusMethodNotAllowed)
		return
	}

	hours := 24
	if h := r.URL.Query().Get("hours"); h != "" {
		if parsed, err := strconv.Atoi(h); err == nil && parsed > 0 {
			hours = parsed
		}
	}

	startTime := time.Now().Add(-time.Duration(hours) * time.Hour)

	var records []SensorData
	if err := db.Where("created_at >= ?", startTime).Order("created_at asc").Find(&records).Error; err != nil {
		http.Error(w, "获取数据失败", http.StatusInternalServerError)
		return
	}

	if len(records) == 0 {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]any{"message": "暂无数据"})
		return
	}

	// 提取数据用于相关性分析
	temps, humidities := make([]float64, len(records)), make([]float64, len(records))
	pm25s, hchos, vocs, co2s := make([]float64, len(records)), make([]float64, len(records)), make([]float64, len(records)), make([]float64, len(records))

	for i, r := range records {
		temps[i] = r.Temperature
		humidities[i] = r.Humidity
		pm25s[i] = r.PM25
		hchos[i] = r.HCHO
		vocs[i] = r.VOC
		co2s[i] = r.CO2
	}

	// 计算相关性
	correlations := map[string]float64{
		"temp_hcho":     calculateCorrelation(temps, hchos),
		"humidity_hcho": calculateCorrelation(humidities, hchos),
		"temp_voc":      calculateCorrelation(temps, vocs),
		"humidity_voc":  calculateCorrelation(humidities, vocs),
		"pm25_particle": calculateCorrelation(pm25s, make([]float64, len(records))), // 需要particles
	}

	// 按小时聚合趋势数据
	hourlyData := make(map[int][]SensorData)
	for _, record := range records {
		hour := record.CreatedAt.Hour()
		hourlyData[hour] = append(hourlyData[hour], record)
	}

	// 计算每小时平均值
	hourlyAvg := make([]map[string]any, 0)
	for hour := 0; hour < 24; hour++ {
		if data, ok := hourlyData[hour]; ok && len(data) > 0 {
			avgPM25, avgCO2, avgHCHO, avgVOC, avgTemp, avgHumidity := 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
			for _, d := range data {
				avgPM25 += d.PM25
				avgCO2 += d.CO2
				avgHCHO += d.HCHO
				avgVOC += d.VOC
				avgTemp += d.Temperature
				avgHumidity += d.Humidity
			}
			n := float64(len(data))
			hourlyAvg = append(hourlyAvg, map[string]any{
				"hour":        hour,
				"pm25":        math.Round(avgPM25/n*100) / 100,
				"co2":         math.Round(avgCO2/n*100) / 100,
				"hcho":        math.Round(avgHCHO/n*100) / 100,
				"voc":         math.Round(avgVOC/n*100) / 100,
				"temperature": math.Round(avgTemp/n*100) / 100,
				"humidity":    math.Round(avgHumidity/n*100) / 100,
				"count":       len(data),
			})
		}
	}

	// 找出峰值时段
	var peakPM25Hour, peakCO2Hour int
	var maxPM25, maxCO2 float64
	for _, avg := range hourlyAvg {
		if pm25 := avg["pm25"].(float64); pm25 > maxPM25 {
			maxPM25 = pm25
			peakPM25Hour = avg["hour"].(int)
		}
		if co2 := avg["co2"].(float64); co2 > maxCO2 {
			maxCO2 = co2
			peakCO2Hour = avg["hour"].(int)
		}
	}

	// 环境建议
	latest := records[len(records)-1]
	suggestions := generateSuggestions(latest)

	// AQI 计算 (简化版)
	aqi := calculateAQI(latest.PM25, latest.CO2, latest.VOC)

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{
		"hours":        hours,
		"correlations": correlations,
		"hourly_trend": hourlyAvg,
		"peak_hours": map[string]any{
			"pm25": map[string]any{"hour": peakPM25Hour, "value": maxPM25},
			"co2":  map[string]any{"hour": peakCO2Hour, "value": maxCO2},
		},
		"aqi":         aqi,
		"suggestions": suggestions,
		"latest":      latest,
	})
}

// calculateAQI 计算空气质量指数
func calculateAQI(pm25, co2, voc float64) map[string]any {
	// PM2.5 AQI 计算 (简化版，基于中国标准)
	var pm25AQI float64
	var pm25Level string
	switch {
	case pm25 <= 35:
		pm25AQI = pm25 * 50 / 35
		pm25Level = "优"
	case pm25 <= 75:
		pm25AQI = 50 + (pm25-35)*50/40
		pm25Level = "良"
	case pm25 <= 115:
		pm25AQI = 100 + (pm25-75)*50/40
		pm25Level = "轻度污染"
	case pm25 <= 150:
		pm25AQI = 150 + (pm25-115)*50/35
		pm25Level = "中度污染"
	case pm25 <= 250:
		pm25AQI = 200 + (pm25-150)*100/100
		pm25Level = "重度污染"
	default:
		pm25AQI = 300 + (pm25-250)*100/100
		pm25Level = "严重污染"
	}

	// CO2 等级
	var co2Level string
	switch {
	case co2 <= 450:
		co2Level = "优秀"
	case co2 <= 700:
		co2Level = "良好"
	case co2 <= 1000:
		co2Level = "一般"
	case co2 <= 2000:
		co2Level = "较差"
	default:
		co2Level = "很差"
	}

	// VOC 等级
	var vocLevel string
	switch {
	case voc <= 200:
		vocLevel = "优秀"
	case voc <= 400:
		vocLevel = "良好"
	case voc <= 600:
		vocLevel = "一般"
	default:
		vocLevel = "较差"
	}

	// 综合评分 (0-100)
	score := 100.0
	if pm25AQI > 50 {
		score -= (pm25AQI - 50) * 0.5
	}
	if co2 > 700 {
		score -= (co2 - 700) * 0.02
	}
	if voc > 300 {
		score -= (voc - 300) * 0.05
	}
	if score < 0 {
		score = 0
	}

	var overallLevel string
	switch {
	case score >= 80:
		overallLevel = "优秀"
	case score >= 60:
		overallLevel = "良好"
	case score >= 40:
		overallLevel = "一般"
	default:
		overallLevel = "较差"
	}

	return map[string]any{
		"pm25_aqi":      math.Round(pm25AQI),
		"pm25_level":    pm25Level,
		"co2_level":     co2Level,
		"voc_level":     vocLevel,
		"overall_score": math.Round(score),
		"overall_level": overallLevel,
	}
}

// generateSuggestions 生成环境建议
func generateSuggestions(data SensorData) []map[string]any {
	suggestions := []map[string]any{}

	// 温度建议
	if data.Temperature < 18 {
		suggestions = append(suggestions, map[string]any{
			"type":    "temperature",
			"icon":    "cold",
			"message": "室温偏低，建议适当增加保暖",
		})
	} else if data.Temperature > 26 {
		suggestions = append(suggestions, map[string]any{
			"type":    "temperature",
			"icon":    "hot",
			"message": "室温偏高，建议开启空调或通风降温",
		})
	}

	// 湿度建议
	if data.Humidity < 30 {
		suggestions = append(suggestions, map[string]any{
			"type":    "humidity",
			"icon":    "dry",
			"message": "空气干燥，建议使用加湿器",
		})
	} else if data.Humidity > 70 {
		suggestions = append(suggestions, map[string]any{
			"type":    "humidity",
			"icon":    "wet",
			"message": "湿度过高，建议通风或使用除湿机",
		})
	}

	// CO2 建议
	if data.CO2 > 1000 {
		suggestions = append(suggestions, map[string]any{
			"type":    "co2",
			"icon":    "ventilation",
			"message": "CO2浓度偏高，建议开窗通风",
		})
	}

	// PM2.5 建议
	if data.PM25 > 75 {
		suggestions = append(suggestions, map[string]any{
			"type":    "pm25",
			"icon":    "air",
			"message": "PM2.5超标，建议使用空气净化器",
		})
	}

	// 甲醛建议
	if data.HCHO > 80 {
		suggestions = append(suggestions, map[string]any{
			"type":    "hcho",
			"icon":    "warning",
			"message": "甲醛偏高，建议开窗通风并检查污染源",
		})
	}

	// VOC 建议
	if data.VOC > 500 {
		suggestions = append(suggestions, map[string]any{
			"type":    "voc",
			"icon":    "warning",
			"message": "VOC偏高，建议通风换气",
		})
	}

	// 舒适度评估
	if data.Temperature >= 20 && data.Temperature <= 24 && data.Humidity >= 40 && data.Humidity <= 60 {
		suggestions = append(suggestions, map[string]any{
			"type":    "comfort",
			"icon":    "check",
			"message": "当前温湿度处于舒适区间",
		})
	}

	if len(suggestions) == 0 {
		suggestions = append(suggestions, map[string]any{
			"type":    "good",
			"icon":    "check",
			"message": "室内环境良好，无需调整",
		})
	}

	return suggestions
}
