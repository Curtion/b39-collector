<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps<{
  aqi: {
    pm25_aqi: number
    pm25_level: string
    co2_level: string
    voc_level: string
    overall_score: number
    overall_level: string
  } | null
}>()

const scoreColor = computed(() => {
  if (!props.aqi) {
    return 'text-gray-400'
  }
  const score = props.aqi.overall_score
  if (score >= 80) {
    return 'text-green-500'
  }
  if (score >= 60) {
    return 'text-blue-500'
  }
  if (score >= 40) {
    return 'text-yellow-500'
  }
  return 'text-red-500'
})

const scoreBg = computed(() => {
  if (!props.aqi) {
    return 'bg-gray-100'
  }
  const score = props.aqi.overall_score
  if (score >= 80) {
    return 'bg-green-50'
  }
  if (score >= 60) {
    return 'bg-blue-50'
  }
  if (score >= 40) {
    return 'bg-yellow-50'
  }
  return 'bg-red-50'
})

function getLevelColor(level: string): string {
  const map: Record<string, string> = {
    优: 'text-green-500 bg-green-50',
    优秀: 'text-green-500 bg-green-50',
    良: 'text-blue-500 bg-blue-50',
    良好: 'text-blue-500 bg-blue-50',
    一般: 'text-yellow-500 bg-yellow-50',
    轻度污染: 'text-orange-500 bg-orange-50',
    中度污染: 'text-orange-600 bg-orange-50',
    较差: 'text-orange-500 bg-orange-50',
    重度污染: 'text-red-500 bg-red-50',
    严重污染: 'text-red-600 bg-red-50',
    很差: 'text-red-500 bg-red-50',
  }
  return map[level] || 'text-gray-500 bg-gray-50'
}
</script>

<template>
  <div class="aqi-card rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <h3 class="mb-3 flex items-center gap-2 text-base font-bold sm:mb-4 sm:text-lg">
      <span class="i-carbon-air-quality text-green-500" />
      空气质量评估
    </h3>

    <div v-if="!aqi" class="py-4 text-center text-gray-400">
      暂无数据
    </div>

    <div v-else>
      <!-- 综合评分 -->
      <div class="mb-4 text-center sm:mb-6" :class="scoreBg" style="border-radius: 12px; padding: 16px;">
        <div class="mb-1.5 text-xs text-gray-500 sm:mb-2 sm:text-sm">
          综合环境评分
        </div>
        <div class="text-4xl font-bold sm:text-5xl" :class="scoreColor">
          {{ aqi.overall_score }}
        </div>
        <div class="mt-1.5 text-sm font-medium sm:mt-2 sm:text-lg" :class="scoreColor">
          {{ aqi.overall_level }}
        </div>
      </div>

      <!-- 各项指标 -->
      <div class="space-y-2 sm:space-y-3">
        <div class="flex items-center justify-between text-sm sm:text-base">
          <span class="text-gray-600">PM2.5 AQI</span>
          <div class="flex items-center gap-2">
            <span class="font-medium">{{ aqi.pm25_aqi }}</span>
            <span class="rounded-full px-2 py-0.5 text-xs" :class="getLevelColor(aqi.pm25_level)">
              {{ aqi.pm25_level }}
            </span>
          </div>
        </div>

        <div class="flex items-center justify-between text-sm sm:text-base">
          <span class="text-gray-600">CO2 等级</span>
          <span class="rounded-full px-2 py-0.5 text-xs" :class="getLevelColor(aqi.co2_level)">
            {{ aqi.co2_level }}
          </span>
        </div>

        <div class="flex items-center justify-between text-sm sm:text-base">
          <span class="text-gray-600">VOC 等级</span>
          <span class="rounded-full px-2 py-0.5 text-xs" :class="getLevelColor(aqi.voc_level)">
            {{ aqi.voc_level }}
          </span>
        </div>
      </div>
    </div>
  </div>
</template>
