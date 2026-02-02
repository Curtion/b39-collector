<script setup lang="ts">
defineProps<{
  correlations: Record<string, number> | null
}>()

const correlationItems = [
  { key: 'temp_hcho', label: '温度 ↔ 甲醛', desc: '高温可能加速甲醛释放' },
  { key: 'humidity_hcho', label: '湿度 ↔ 甲醛', desc: '高湿可能加速甲醛释放' },
  { key: 'temp_voc', label: '温度 ↔ VOC', desc: '温度对VOC挥发的影响' },
  { key: 'humidity_voc', label: '湿度 ↔ VOC', desc: '湿度对VOC的影响' },
]

function getCorrelationLevel(value: number): { text: string, color: string } {
  const abs = Math.abs(value)
  if (abs >= 0.7) {
    return { text: '强相关', color: 'text-red-500' }
  }
  if (abs >= 0.4) {
    return { text: '中等相关', color: 'text-yellow-500' }
  }
  if (abs >= 0.2) {
    return { text: '弱相关', color: 'text-blue-500' }
  }
  return { text: '无明显相关', color: 'text-gray-400' }
}

function getCorrelationDirection(value: number): string {
  if (value > 0.1) {
    return '正相关 ↑'
  }
  if (value < -0.1) {
    return '负相关 ↓'
  }
  return '无方向'
}
</script>

<template>
  <div class="correlation-card rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <h3 class="mb-3 flex items-center gap-2 text-base font-bold sm:mb-4 sm:text-lg">
      <span class="i-carbon-chart-relationship text-purple-500" />
      相关性分析
    </h3>

    <div v-if="!correlations" class="py-4 text-center text-gray-400">
      暂无数据
    </div>

    <div v-else class="space-y-3 sm:space-y-4">
      <div
        v-for="item in correlationItems"
        :key="item.key"
        class="rounded-lg border border-gray-100 p-3 sm:p-4"
      >
        <div class="mb-2 flex flex-col gap-1 sm:flex-row sm:items-center sm:justify-between">
          <span class="font-medium text-sm sm:text-base">{{ item.label }}</span>
          <div class="flex items-center gap-2">
            <span class="font-mono text-base sm:text-lg">
              {{ (correlations[item.key] ?? 0).toFixed(3) }}
            </span>
            <span
              class="rounded-full px-2 py-0.5 text-xs"
              :class="getCorrelationLevel(correlations[item.key] ?? 0).color"
            >
              {{ getCorrelationLevel(correlations[item.key] ?? 0).text }}
            </span>
          </div>
        </div>
        <div class="flex items-center justify-between text-xs text-gray-500 sm:text-sm">
          <span>{{ item.desc }}</span>
          <span>{{ getCorrelationDirection(correlations[item.key] ?? 0) }}</span>
        </div>
        <!-- 进度条可视化 -->
        <div class="mt-2 h-1.5 overflow-hidden rounded-full bg-gray-100">
          <div
            class="h-full rounded-full transition-all"
            :class="(correlations[item.key] ?? 0) >= 0 ? 'bg-green-400' : 'bg-red-400'"
            :style="{ width: `${Math.abs(correlations[item.key] ?? 0) * 100}%` }"
          />
        </div>
      </div>

      <div class="mt-3 rounded-lg bg-blue-50 p-2 text-xs text-blue-700 sm:mt-4 sm:p-3 sm:text-sm">
        <span class="i-carbon-information mr-1" />
        相关系数范围 -1 到 1，绝对值越大表示相关性越强。正值表示正相关（同增同减），负值表示负相关（此消彼长）。
      </div>
    </div>
  </div>
</template>
