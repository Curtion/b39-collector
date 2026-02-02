<script setup lang="ts">
import Anomalies from './components/Anomalies.vue'
import AqiCard from './components/AqiCard.vue'
import Co2TrendChart from './components/Co2TrendChart.vue'
import CorrelationCard from './components/CorrelationCard.vue'
import HchoTrendChart from './components/HchoTrendChart.vue'
import Pm25TrendChart from './components/Pm25TrendChart.vue'
import RealtimeData from './components/RealtimeData.vue'
import StatsTable from './components/StatsTable.vue'
import Suggestions from './components/Suggestions.vue'
import TrendChart from './components/TrendChart.vue'
import { useSensor } from './composables/useSensor'

const {
  latestData,
  historyData,
  stats,
  analysis,
  loading,
  refreshAll,
} = useSensor()
</script>

<template>
  <div class="min-h-screen bg-gray-50 dark:bg-gray-900">
    <!-- 顶部导航 -->
    <header class="sticky top-0 z-50 border-b border-gray-200 bg-white/80 px-3 py-3 backdrop-blur-sm dark:border-gray-700 dark:bg-gray-800/80 sm:px-6 sm:py-4">
      <div class="mx-auto flex max-w-7xl items-center justify-between">
        <div class="flex items-center gap-2 sm:gap-3">
          <span class="i-carbon-air-quality text-2xl text-blue-500 sm:text-3xl" />
          <h1 class="text-base font-bold text-gray-800 dark:text-white sm:text-xl">
            B39 传感器监测系统
          </h1>
        </div>
        <button
          class="flex items-center gap-1 rounded-lg bg-blue-500 px-3 py-1.5 text-sm text-white transition-colors hover:bg-blue-600 disabled:opacity-50 sm:gap-2 sm:px-4 sm:py-2"
          :disabled="loading"
          @click="refreshAll"
        >
          <span class="i-carbon-renew text-base" :class="{ 'animate-spin': loading }" />
          <span class="hidden sm:inline">刷新数据</span>
        </button>
      </div>
    </header>

    <!-- 主内容区 -->
    <main class="mx-auto max-w-7xl px-3 py-4 sm:px-6 sm:py-6">
      <!-- 实时数据 -->
      <section class="mb-6">
        <RealtimeData :data="latestData" />
      </section>

      <!-- AQI + 建议 + 告警 -->
      <section class="mb-4 grid gap-4 lg:grid-cols-3 sm:mb-6 sm:gap-6">
        <AqiCard :aqi="analysis?.aqi ?? null" />
        <Suggestions :suggestions="analysis?.suggestions ?? null" />
        <Anomalies :anomalies="stats?.anomalies ?? null" />
      </section>

      <!-- 趋势图表 -->
      <section class="mb-4 grid gap-4 lg:grid-cols-2 sm:mb-6 sm:gap-6">
        <TrendChart :data="historyData" title="历史数据趋势" />
        <HchoTrendChart :data="historyData" />
      </section>

      <!-- CO2 和 PM2.5 趋势图 -->
      <section class="mb-4 grid gap-4 lg:grid-cols-2 sm:mb-6 sm:gap-6">
        <Co2TrendChart :data="historyData" />
        <Pm25TrendChart :data="historyData" />
      </section>

      <!-- 统计数据 -->
      <section class="mb-4 sm:mb-6">
        <StatsTable :stats="stats?.stats ?? null" :hours="stats?.hours ?? 24" />
      </section>

      <!-- 相关性分析 -->
      <section class="mb-4 sm:mb-6">
        <CorrelationCard :correlations="analysis?.correlations ?? null" />
      </section>
    </main>

    <!-- 页脚 -->
    <footer class="border-t border-gray-200 bg-white px-3 py-3 text-center text-xs text-gray-500 dark:border-gray-700 dark:bg-gray-800 sm:px-6 sm:py-4 sm:text-sm">
      B39 Collector © 2026 | 数据每 5 秒自动刷新
    </footer>
  </div>
</template>

<style lang="scss" scoped>
// 动画
@keyframes spin {
  from {
    transform: rotate(0deg);
  }
  to {
    transform: rotate(360deg);
  }
}

.animate-spin {
  animation: spin 1s linear infinite;
}
</style>
