<script setup lang="ts">
import type { StatsResult } from '../composables/useSensor'

defineProps<{
  stats: {
    particle: StatsResult
    pm25: StatsResult
    hcho: StatsResult
    co2: StatsResult
    temperature: StatsResult
    humidity: StatsResult
    voc: StatsResult
  } | null
  hours: number
}>()

const statItems = [
  { key: 'pm25', label: 'PM2.5', unit: 'μg/m³', color: 'text-blue-500' },
  { key: 'co2', label: 'CO2', unit: 'PPM', color: 'text-green-500' },
  { key: 'hcho', label: '甲醛', unit: 'μg/m³', color: 'text-yellow-500' },
  { key: 'voc', label: 'VOC', unit: 'ppb', color: 'text-purple-500' },
  { key: 'temperature', label: '温度', unit: '℃', color: 'text-red-500' },
  { key: 'humidity', label: '湿度', unit: '%', color: 'text-cyan-500' },
  { key: 'particle', label: '0.3μm颗粒物', unit: 'pcs/0.1L', color: 'text-pink-500' },
]

function getStat(stats: any, key: string): StatsResult | null {
  return stats?.[key] || null
}
</script>

<template>
  <div class="stats-table rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <h3 class="mb-3 flex items-center gap-2 text-base font-bold sm:mb-4 sm:text-lg">
      <span class="i-carbon-analytics text-blue-500" />
      统计数据
      <span class="text-xs font-normal text-gray-400 sm:text-sm">(过去 {{ hours }} 小时)</span>
    </h3>

    <div v-if="!stats" class="py-4 text-center text-gray-400">
      暂无数据
    </div>

    <div v-else class="overflow-x-auto -mx-3 sm:mx-0">
      <div class="px-3 sm:px-0">
        <table class="w-full text-xs sm:text-sm">
          <thead>
            <tr class="border-b text-left text-gray-500">
              <th class="pb-2 pr-2 sm:pb-3 sm:pr-4">
                指标
              </th>
              <th class="pb-2 pr-2 text-right sm:pb-3 sm:pr-4">
                最小
              </th>
              <th class="pb-2 pr-2 text-right sm:pb-3 sm:pr-4">
                最大
              </th>
              <th class="pb-2 pr-2 text-right sm:pb-3 sm:pr-4">
                平均
              </th>
              <th class="pb-2 pr-2 text-right sm:pb-3 sm:pr-4">
                中位
              </th>
              <th class="pb-2 text-right sm:pb-3">
                标准差
              </th>
            </tr>
          </thead>
          <tbody>
            <tr
              v-for="item in statItems"
              :key="item.key"
              class="border-b border-gray-100 last:border-0"
            >
              <td class="py-2 pr-2 sm:py-3 sm:pr-4">
                <span :class="item.color" class="font-medium">{{ item.label }}</span>
                <span class="ml-1 hidden text-gray-400 sm:inline">({{ item.unit }})</span>
              </td>
              <td class="py-2 pr-2 text-right font-mono sm:py-3 sm:pr-4">
                {{ getStat(stats, item.key)?.min ?? '-' }}
              </td>
              <td class="py-2 pr-2 text-right font-mono sm:py-3 sm:pr-4">
                {{ getStat(stats, item.key)?.max ?? '-' }}
              </td>
              <td class="py-2 pr-2 text-right font-mono font-medium sm:py-3 sm:pr-4">
                {{ getStat(stats, item.key)?.avg ?? '-' }}
              </td>
              <td class="py-2 pr-2 text-right font-mono sm:py-3 sm:pr-4">
                {{ getStat(stats, item.key)?.median ?? '-' }}
              </td>
              <td class="py-2 text-right font-mono text-gray-500 sm:py-3">
                {{ getStat(stats, item.key)?.std_dev ?? '-' }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
</template>
