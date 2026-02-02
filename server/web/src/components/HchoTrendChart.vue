<script setup lang="ts">
import type { SensorData } from '../composables/useSensor'
import { LineChart } from 'echarts/charts'
import { DataZoomComponent, GridComponent, MarkLineComponent, TitleComponent, TooltipComponent } from 'echarts/components'
import { use } from 'echarts/core'
import { CanvasRenderer } from 'echarts/renderers'
import { computed } from 'vue'
import VChart from 'vue-echarts'

const props = defineProps<{
  data: SensorData[]
}>()

use([LineChart, GridComponent, TitleComponent, TooltipComponent, DataZoomComponent, MarkLineComponent, CanvasRenderer])

const option = computed(() => {
  if (!props.data || props.data.length === 0) {
    return {}
  }

  const times = props.data.map(d => new Date(d.created_at).toLocaleTimeString())
  const hchoValues = props.data.map(d => d.hcho)

  return {
    title: {
      text: '甲醛浓度趋势',
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
        if (value > 100) {
          level = '超标'
          color = '#ef4444'
        } else if (value > 80) {
          level = '轻度超标'
          color = '#f59e0b'
        } else if (value > 50) {
          level = '一般'
          color = '#eab308'
        }
        return `${data.axisValue}<br/>
          甲醛: <span style="color:${color};font-weight:bold">${value} μg/m³</span><br/>
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
      name: '甲醛 (μg/m³)',
      min: 0,
    },
    series: [
      {
        name: '甲醛',
        type: 'line',
        smooth: true,
        data: hchoValues,
        itemStyle: { color: '#8b5cf6' },
        lineStyle: { width: 2 },
        areaStyle: {
          color: {
            type: 'linear',
            x: 0,
            y: 0,
            x2: 0,
            y2: 1,
            colorStops: [
              { offset: 0, color: 'rgba(139, 92, 246, 0.3)' },
              { offset: 1, color: 'rgba(139, 92, 246, 0.05)' },
            ],
          },
        },
        markLine: {
          silent: true,
          data: [
            {
              yAxis: 80,
              lineStyle: { color: '#f59e0b', type: 'dashed' },
              label: { formatter: '轻度超标 (80 μg/m³)', position: 'insideEndTop' },
            },
            {
              yAxis: 100,
              lineStyle: { color: '#ef4444', type: 'dashed' },
              label: { formatter: '超标 (100 μg/m³)', position: 'insideEndTop' },
            },
          ],
        },
      },
    ],
  }
})
</script>

<template>
  <div class="hcho-trend-chart rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <div v-if="!data || data.length === 0" class="flex h-60 items-center justify-center text-gray-400 sm:h-80">
      暂无数据
    </div>
    <VChart v-else :option="option" style="height: 250px;" class="sm:!h-[350px]" autoresize />
  </div>
</template>
