<script setup lang="ts">
import type { SensorData } from '../composables/useSensor'
import { LineChart } from 'echarts/charts'
import { DataZoomComponent, GridComponent, LegendComponent, TitleComponent, TooltipComponent } from 'echarts/components'
import { use } from 'echarts/core'
import { CanvasRenderer } from 'echarts/renderers'
import { computed } from 'vue'
import VChart from 'vue-echarts'

const props = defineProps<{
  data: SensorData[]
  title?: string
}>()

use([LineChart, GridComponent, LegendComponent, TitleComponent, TooltipComponent, DataZoomComponent, CanvasRenderer])

const option = computed(() => {
  if (!props.data || props.data.length === 0) {
    return {}
  }

  const times = props.data.map(d => new Date(d.created_at).toLocaleTimeString())

  return {
    title: {
      text: props.title || '历史趋势',
      left: 'center',
      textStyle: {
        fontSize: 16,
        fontWeight: 'bold',
      },
    },
    tooltip: {
      trigger: 'axis',
      axisPointer: {
        type: 'cross',
      },
    },
    legend: {
      data: ['PM2.5', 'CO2', '甲醛', 'VOC', '温度', '湿度'],
      top: 50,
    },
    grid: {
      left: '3%',
      right: '4%',
      bottom: '15%',
      top: 100,
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
    yAxis: [
      {
        type: 'value',
        name: '浓度',
        position: 'left',
      },
      {
        type: 'value',
        name: '温湿度',
        position: 'right',
        max: 100,
      },
    ],
    series: [
      {
        name: 'PM2.5',
        type: 'line',
        smooth: true,
        data: props.data.map(d => d.pm25),
        itemStyle: { color: '#3b82f6' },
        lineStyle: { width: 2 },
      },
      {
        name: 'CO2',
        type: 'line',
        smooth: true,
        data: props.data.map(d => d.co2 / 10), // 缩放以便显示
        itemStyle: { color: '#22c55e' },
        lineStyle: { width: 2 },
      },
      {
        name: '甲醛',
        type: 'line',
        smooth: true,
        data: props.data.map(d => d.hcho),
        itemStyle: { color: '#f59e0b' },
        lineStyle: { width: 2 },
      },
      {
        name: 'VOC',
        type: 'line',
        smooth: true,
        data: props.data.map(d => d.voc / 5), // 缩放以便显示
        itemStyle: { color: '#8b5cf6' },
        lineStyle: { width: 2 },
      },
      {
        name: '温度',
        type: 'line',
        smooth: true,
        yAxisIndex: 1,
        data: props.data.map(d => d.temperature),
        itemStyle: { color: '#ef4444' },
        lineStyle: { width: 2, type: 'dashed' },
      },
      {
        name: '湿度',
        type: 'line',
        smooth: true,
        yAxisIndex: 1,
        data: props.data.map(d => d.humidity),
        itemStyle: { color: '#06b6d4' },
        lineStyle: { width: 2, type: 'dashed' },
      },
    ],
  }
})
</script>

<template>
  <div class="trend-chart rounded-xl bg-white p-3 shadow-sm dark:bg-gray-800 sm:p-6">
    <div v-if="!data || data.length === 0" class="flex h-60 items-center justify-center text-gray-400 sm:h-80">
      暂无数据
    </div>
    <VChart v-else :option="option" style="height: 250px;" class="sm:!h-[400px]" autoresize />
  </div>
</template>
