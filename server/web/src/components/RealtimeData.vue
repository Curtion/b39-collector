<script setup lang="ts">
import type { SensorData } from '../composables/useSensor'

const props = defineProps<{
  data: SensorData | null
}>()

// 数据项配置
const dataItems = [
  { key: 'pm25', label: 'PM2.5', unit: 'μg/m³', icon: 'i-carbon-smoke', color: '#3b82f6', thresholds: [35, 75, 150] },
  { key: 'co2', label: 'CO2', unit: 'PPM', icon: 'i-carbon-cloud', color: '#22c55e', thresholds: [700, 1000, 2000] },
  { key: 'hcho', label: '甲醛', unit: 'μg/m³', icon: 'i-carbon-chemistry', color: '#f59e0b', thresholds: [50, 80, 100] },
  { key: 'voc', label: 'VOC', unit: 'ppb', icon: 'i-carbon-warning-alt', color: '#8b5cf6', thresholds: [300, 500, 800] },
  { key: 'temperature', label: '温度', unit: '℃', icon: 'i-carbon-temperature', color: '#ef4444', thresholds: [18, 26, 30] },
  { key: 'humidity', label: '湿度', unit: '%', icon: 'i-carbon-humidity', color: '#06b6d4', thresholds: [30, 60, 80] },
  { key: 'particle', label: '0.3μm颗粒物', unit: 'pcs/0.1L', icon: 'i-carbon-circle-dash', color: '#ec4899', thresholds: [1000, 3000, 5000] },
]

// 获取状态颜色
function getStatusColor(key: string, value: number): string {
  const item = dataItems.find(i => i.key === key)
  if (!item) {
    return 'text-gray-500'
  }

  const thresholds = item.thresholds as [number, number, number]
  const low = thresholds[0]
  const mid = thresholds[1]
  const high = thresholds[2]

  // 温度和湿度的特殊处理
  if (key === 'temperature') {
    if (value >= 20 && value <= 24) {
      return 'text-green-500'
    }
    if (value < 18 || value > 26) {
      return 'text-red-500'
    }
    return 'text-yellow-500'
  }

  if (key === 'humidity') {
    if (value >= 40 && value <= 60) {
      return 'text-green-500'
    }
    if (value < 30 || value > 70) {
      return 'text-red-500'
    }
    return 'text-yellow-500'
  }

  // 其他指标
  if (value <= low) {
    return 'text-green-500'
  }
  if (value <= mid) {
    return 'text-yellow-500'
  }
  if (value <= high) {
    return 'text-orange-500'
  }
  return 'text-red-500'
}

function getValue(key: string): number {
  if (!props.data) {
    return 0
  }
  return (props.data as any)[key] ?? 0
}
</script>

<template>
  <div class="realtime-data">
    <div class="mb-3 flex flex-col gap-2 sm:mb-4 sm:flex-row sm:items-center sm:justify-between">
      <h2 class="flex items-center gap-2 text-lg font-bold sm:text-xl">
        <span class="i-carbon-dashboard text-blue-500" />
        实时数据
      </h2>
      <div v-if="data" class="flex items-center gap-2 text-xs text-gray-500 sm:text-sm">
        <span class="i-carbon-time" />
        <span class="hidden sm:inline">{{ new Date(data.created_at).toLocaleString() }}</span>
        <span class="sm:hidden">{{ new Date(data.created_at).toLocaleTimeString() }}</span>
        <span
          class="ml-2 rounded-full px-2 py-0.5 text-xs"
          :class="data.is_valid ? 'bg-green-100 text-green-600' : 'bg-red-100 text-red-600'"
        >
          {{ data.is_valid ? '正常' : '异常' }}
        </span>
      </div>
    </div>

    <div v-if="!data" class="py-6 text-center text-gray-400 sm:py-8">
      <span class="i-carbon-no-data mb-2 text-3xl sm:text-4xl" />
      <p class="text-sm sm:text-base">
        暂无数据
      </p>
    </div>

    <div v-else class="grid grid-cols-2 gap-2 sm:gap-3 md:grid-cols-4 lg:grid-cols-7">
      <div
        v-for="item in dataItems"
        :key="item.key"
        class="rounded-xl bg-white p-3 shadow-sm transition-shadow hover:shadow-md dark:bg-gray-800 sm:p-4"
      >
        <div class="mb-1.5 flex items-center gap-1.5 text-gray-500 sm:mb-2 sm:gap-2">
          <span :class="item.icon" :style="{ color: item.color }" class="text-base sm:text-lg" />
          <span class="text-xs sm:text-sm">{{ item.label }}</span>
        </div>
        <div class="text-lg font-bold sm:text-2xl" :class="getStatusColor(item.key, getValue(item.key))">
          {{ getValue(item.key).toFixed(1) }}
        </div>
        <div class="text-[10px] text-gray-400 sm:text-xs">
          {{ item.unit }}
        </div>
      </div>
    </div>
  </div>
</template>
