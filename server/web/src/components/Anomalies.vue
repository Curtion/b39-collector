<script setup lang="ts">
defineProps<{
  anomalies: Array<{
    type: string
    value?: number
    threshold?: number
    level: string
    message: string
  }> | null
}>()

function getLevelClass(level: string): string {
  if (level === 'danger') {
    return 'bg-red-50 border-red-300 text-red-700'
  }
  if (level === 'warning') {
    return 'bg-yellow-50 border-yellow-300 text-yellow-700'
  }
  return 'bg-gray-50 border-gray-300 text-gray-700'
}

function getIconClass(level: string): string {
  if (level === 'danger') {
    return 'i-carbon-warning-filled text-red-500'
  }
  if (level === 'warning') {
    return 'i-carbon-warning text-yellow-500'
  }
  return 'i-carbon-information text-gray-500'
}
</script>

<template>
  <div class="anomalies rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <h3 class="mb-3 flex items-center gap-2 text-base font-bold sm:mb-4 sm:text-lg">
      <span class="i-carbon-notification text-red-500" />
      异常告警
    </h3>

    <div v-if="!anomalies || anomalies.length === 0" class="flex items-center gap-2 rounded-lg bg-green-50 p-3 text-sm text-green-700 sm:p-4 sm:text-base">
      <span class="i-carbon-checkmark-filled text-lg sm:text-xl" />
      <span>所有指标正常，无异常告警</span>
    </div>

    <div v-else class="space-y-2 sm:space-y-3">
      <div
        v-for="(item, index) in anomalies"
        :key="index"
        class="flex items-center gap-2 rounded-lg border p-2.5 sm:gap-3 sm:p-3"
        :class="getLevelClass(item.level)"
      >
        <span :class="getIconClass(item.level)" class="flex-shrink-0 text-lg sm:text-xl" />
        <div class="flex-1">
          <div class="text-sm font-medium sm:text-base">
            {{ item.message }}
          </div>
          <div v-if="item.value !== undefined" class="mt-0.5 text-xs opacity-75 sm:mt-1 sm:text-sm">
            当前值: {{ item.value }} | 阈值: {{ item.threshold }}
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
