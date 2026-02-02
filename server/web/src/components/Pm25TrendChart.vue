<script setup lang="ts">
import type { SensorData } from '../composables/useSensor'
import { LineChart } from 'echarts/charts'
import { DataZoomComponent, GridComponent, MarkLineComponent, TitleComponent, TooltipComponent, VisualMapComponent } from 'echarts/components'
import { use } from 'echarts/core'
import { CanvasRenderer } from 'echarts/renderers'
import { computed } from 'vue'
import VChart from 'vue-echarts'

const props = defineProps<{
  data: SensorData[]
}>()

use([LineChart, GridComponent, TitleComponent, TooltipComponent, DataZoomComponent, MarkLineComponent, VisualMapComponent, CanvasRenderer])

// 获取 AQI 等级
function getAqiLevel(pm25: number) {
  if (pm25 <= 35) {
    return { level: '优', color: '#22c55e' }
  }
  if (pm25 <= 75) {
    return { level: '良', color: '#84cc16' }
  }
  if (pm25 <= 115) {
    return { level: '轻度污染', color: '#f59e0b' }
  }
  if (pm25 <= 150) {
    return { level: '中度污染', color: '#f97316' }
  }
  if (pm25 <= 250) {
    return { level: '重度污染', color: '#ef4444' }
  }
  return { level: '严重污染', color: '#7f1d1d' }
}

const option = computed(() => {
  if (!props.data || props.data.length === 0) {
    return {}
  }

  const times = props.data.map(d => new Date(d.created_at).toLocaleTimeString())
  const pm25Values = props.data.map(d => d.pm25)

  return {
    title: {
      text: 'PM2.5 浓度趋势',
      left: 'center',
      textStyle: {
        fontSize: 16,
        fontWeight: 'bold',
      },
    },
    tooltip: {
      trigger: 'axis',
      formatter: (params: any) => {
        const data = params[0]
        const value = data.value
        const { level, color } = getAqiLevel(value)
        return `${data.axisValue}<br/>
          PM2.5: <span style="color:${color};font-weight:bold">${value} μg/m³</span><br/>
          等级: <span style="color:${color}">${level}</span>`
      },
    },
    grid: {
      left: '3%',
      right: '4%',
      bottom: '15%',
      top: 60,
      containLabel: true,
    },
    dataZoom: [
      {
        type: 'inside',
        start: 0,
        end: 100,
      },
      {
        start: 0,
        end: 100,
      },
    ],
    xAxis: {
      type: 'category',
      boundaryGap: false,
      data: times,
      axisLabel: {
        rotate: 45,
      },
    },
    yAxis: {
      type: 'value',
      name: 'PM2.5 (μg/m³)',
      min: 0,
    },
    series: [
      {
        name: 'PM2.5',
        type: 'line',
        smooth: true,
        data: pm25Values,
        itemStyle: { color: '#3b82f6' },
        lineStyle: { width: 2 },
        areaStyle: {
          color: {
            type: 'linear',
            x: 0,
            y: 0,
            x2: 0,
            y2: 1,
            colorStops: [
              { offset: 0, color: 'rgba(59, 130, 246, 0.3)' },
              { offset: 1, color: 'rgba(59, 130, 246, 0.05)' },
            ],
          },
        },
        markLine: {
          silent: true,
          data: [
            {
              yAxis: 35,
              lineStyle: { color: '#84cc16', type: 'dashed' },
              label: { formatter: '良 (35)', position: 'insideEndTop' },
            },
            {
              yAxis: 75,
              lineStyle: { color: '#f59e0b', type: 'dashed' },
              label: { formatter: '轻度污染 (75)', position: 'insideEndTop' },
            },
            {
              yAxis: 115,
              lineStyle: { color: '#ef4444', type: 'dashed' },
              label: { formatter: '中度污染 (115)', position: 'insideEndTop' },
            },
          ],
        },
      },
    ],
    visualMap: {
      show: false,
      pieces: [
        { lte: 35, color: '#22c55e' },
        { gt: 35, lte: 75, color: '#84cc16' },
        { gt: 75, lte: 115, color: '#f59e0b' },
        { gt: 115, lte: 150, color: '#f97316' },
        { gt: 150, color: '#ef4444' },
      ],
    },
  }
})
</script>

<template>
  <div class="pm25-trend-chart rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <div v-if="!data || data.length === 0" class="flex h-60 items-center justify-center text-gray-400 sm:h-80">
      暂无数据
    </div>
    <VChart v-else :option="option" style="height: 250px;" class="sm:!h-[350px]" autoresize />
  </div>
</template>
