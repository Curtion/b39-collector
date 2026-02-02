import { onMounted, onUnmounted, ref } from 'vue'

export interface SensorData {
  id: number
  created_at: string
  particle: number
  pm25: number
  hcho: number
  co2: number
  temperature: number
  humidity: number
  voc: number
  sequence_num: number
  is_valid: boolean
}

export interface StatsResult {
  min: number
  max: number
  avg: number
  median: number
  std_dev: number
  count: number
}

export interface StatsResponse {
  hours: number
  count: number
  start_time: string
  end_time: string
  stats: {
    particle: StatsResult
    pm25: StatsResult
    hcho: StatsResult
    co2: StatsResult
    temperature: StatsResult
    humidity: StatsResult
    voc: StatsResult
  }
  anomalies: Array<{
    type: string
    value?: number
    threshold?: number
    level: string
    message: string
  }>
}

export interface AnalysisResponse {
  hours: number
  correlations: Record<string, number>
  hourly_trend: Array<{
    hour: number
    pm25: number
    co2: number
    hcho: number
    voc: number
    temperature: number
    humidity: number
    count: number
  }>
  peak_hours: {
    pm25: { hour: number, value: number }
    co2: { hour: number, value: number }
  }
  aqi: {
    pm25_aqi: number
    pm25_level: string
    co2_level: string
    voc_level: string
    overall_score: number
    overall_level: string
  }
  suggestions: Array<{
    type: string
    icon: string
    message: string
  }>
  latest: SensorData
}

export interface HistoryResponse {
  count: number
  data: SensorData[]
}

export function useSensor() {
  const latestData = ref<SensorData | null>(null)
  const historyData = ref<SensorData[]>([])
  const stats = ref<StatsResponse | null>(null)
  const analysis = ref<AnalysisResponse | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  let refreshInterval: ReturnType<typeof setInterval> | null = null

  // 获取最新状态
  async function fetchStatus() {
    try {
      const res = await fetch('/api/status')
      if (res.ok) {
        const data = await res.json()
        latestData.value = data.last_data
      }
    } catch (e) {
      console.error('获取状态失败:', e)
    }
  }

  // 获取历史数据
  async function fetchHistory(hours = 24) {
    try {
      const res = await fetch(`/api/history?hours=${hours}`)
      if (res.ok) {
        const data: HistoryResponse = await res.json()
        historyData.value = data.data.reverse() // 按时间正序
      }
    } catch (e) {
      console.error('获取历史数据失败:', e)
    }
  }

  // 获取统计数据
  async function fetchStats(hours = 24) {
    try {
      const res = await fetch(`/api/stats?hours=${hours}`)
      if (res.ok) {
        stats.value = await res.json()
      }
    } catch (e) {
      console.error('获取统计数据失败:', e)
    }
  }

  // 获取分析数据
  async function fetchAnalysis(hours = 24) {
    try {
      const res = await fetch(`/api/analysis?hours=${hours}`)
      if (res.ok) {
        analysis.value = await res.json()
      }
    } catch (e) {
      console.error('获取分析数据失败:', e)
    }
  }

  // 刷新所有数据
  async function refreshAll() {
    loading.value = true
    error.value = null
    try {
      await Promise.all([
        fetchStatus(),
        fetchHistory(24),
        fetchStats(24),
        fetchAnalysis(24),
      ])
    } catch {
      error.value = '数据加载失败'
    } finally {
      loading.value = false
    }
  }

  // 开始自动刷新
  function startAutoRefresh(intervalMs = 5000) {
    stopAutoRefresh()
    refreshInterval = setInterval(() => {
      fetchStatus()
      fetchStats(24)
    }, intervalMs)
  }

  // 停止自动刷新
  function stopAutoRefresh() {
    if (refreshInterval) {
      clearInterval(refreshInterval)
      refreshInterval = null
    }
  }

  onMounted(() => {
    refreshAll()
    startAutoRefresh()
  })

  onUnmounted(() => {
    stopAutoRefresh()
  })

  return {
    latestData,
    historyData,
    stats,
    analysis,
    loading,
    error,
    fetchStatus,
    fetchHistory,
    fetchStats,
    fetchAnalysis,
    refreshAll,
    startAutoRefresh,
    stopAutoRefresh,
  }
}
