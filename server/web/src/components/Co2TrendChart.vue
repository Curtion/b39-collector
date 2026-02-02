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

const option = computed(() => {
  if (!props.data || props.data.length === 0) {
    return {}
  }

  const times = props.data.map(d => new Date(d.created_at).toLocaleTimeString())
  const co2Values = props.data.map(d => d.co2)

  return {
    title: {
      text: 'CO₂ 浓度趋势',
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
        let level = '优'
        let color = '#22c55e'
        if (value > 2000) {
          level = '差'
          color = '#ef4444'
        } else if (value > 1000) {
          level = '一般'
          color = '#f59e0b'
        }
        return `${data.axisValue}<br/>
          CO₂: <span style="color:${color};font-weight:bold">${value} ppm</span><br/>
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
      name: 'CO₂ (ppm)',
      min: 0,
    },
    series: [
      {
        name: 'CO₂',
        type: 'line',
        smooth: true,
        data: co2Values,
        itemStyle: { color: '#22c55e' },
        lineStyle: { width: 2 },
        areaStyle: {
          color: {
            type: 'linear',
            x: 0,
            y: 0,
            x2: 0,
            y2: 1,
            colorStops: [
              { offset: 0, color: 'rgba(34, 197, 94, 0.3)' },
              { offset: 1, color: 'rgba(34, 197, 94, 0.05)' },
            ],
          },
        },
        markLine: {
          silent: true,
          data: [
            {
              yAxis: 1000,
              lineStyle: { color: '#f59e0b', type: 'dashed' },
              label: { formatter: '一般 (1000 ppm)', position: 'insideEndTop' },
            },
            {
              yAxis: 2000,
              lineStyle: { color: '#ef4444', type: 'dashed' },
              label: { formatter: '差 (2000 ppm)', position: 'insideEndTop' },
            },
          ],
        },
      },
    ],
    visualMap: {
      show: false,
      pieces: [
        { lte: 1000, color: '#22c55e' },
        { gt: 1000, lte: 2000, color: '#f59e0b' },
        { gt: 2000, color: '#ef4444' },
      ],
    },
  }
})
</script>

<template>
  <div class="co2-trend-chart rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <div v-if="!data || data.length === 0" class="flex h-60 items-center justify-center text-gray-400 sm:h-80">
      暂无数据
    </div>
    <VChart v-else :option="option" style="height: 250px;" class="sm:!h-[350px]" autoresize />
  </div>
</template>
