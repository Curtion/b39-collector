<script setup lang="ts">
defineProps<{
  suggestions: Array<{
    type: string
    icon: string
    message: string
  }> | null
}>()

function getIconClass(icon: string): string {
  const map: Record<string, string> = {
    cold: 'i-carbon-snow-blizzard text-blue-500',
    hot: 'i-carbon-temperature-hot text-red-500',
    dry: 'i-carbon-humidity text-yellow-500',
    wet: 'i-carbon-rain text-blue-400',
    ventilation: 'i-carbon-wind-stream text-green-500',
    air: 'i-carbon-air-quality text-purple-500',
    warning: 'i-carbon-warning text-orange-500',
    check: 'i-carbon-checkmark-filled text-green-500',
  }
  return map[icon] || 'i-carbon-information text-gray-500'
}

function getBgClass(type: string): string {
  const map: Record<string, string> = {
    temperature: 'bg-orange-50 border-orange-200',
    humidity: 'bg-blue-50 border-blue-200',
    co2: 'bg-green-50 border-green-200',
    pm25: 'bg-purple-50 border-purple-200',
    hcho: 'bg-yellow-50 border-yellow-200',
    voc: 'bg-pink-50 border-pink-200',
    comfort: 'bg-green-50 border-green-200',
    good: 'bg-green-50 border-green-200',
  }
  return map[type] || 'bg-gray-50 border-gray-200'
}
</script>

<template>
  <div class="suggestions rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <h3 class="mb-3 flex items-center gap-2 text-base font-bold sm:mb-4 sm:text-lg">
      <span class="i-carbon-idea text-yellow-500" />
      环境建议
    </h3>

    <div v-if="!suggestions || suggestions.length === 0" class="py-4 text-center text-gray-400">
      暂无建议
    </div>

    <div v-else class="space-y-2 sm:space-y-3">
      <div
        v-for="(item, index) in suggestions"
        :key="index"
        class="flex items-start gap-2 rounded-lg border p-2.5 sm:gap-3 sm:p-3"
        :class="getBgClass(item.type)"
      >
        <span :class="getIconClass(item.icon)" class="mt-0.5 flex-shrink-0 text-lg sm:text-xl" />
        <span class="text-xs text-gray-700 sm:text-sm">{{ item.message }}</span>
      </div>
    </div>
  </div>
</template>
