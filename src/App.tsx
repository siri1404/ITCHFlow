import React from 'react';
import { Header } from './components/Header';
import { MetricsCard } from './components/MetricsCard';
import { RealtimeChart } from './components/RealtimeChart';
import { ControlPanel } from './components/ControlPanel';
import { MicroburstDetector } from './components/MicroburstDetector';
import { TechnicalSpecs } from './components/TechnicalSpecs';

function App() {
  return (
    <div className="min-h-screen bg-gray-900">
      <Header />
      
      <main className="p-6 space-y-6">
        {/* Key Metrics Row */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          <MetricsCard
            title="Message Throughput"
            value="127.5K"
            subtitle="messages/second"
            trend="up"
            color="green"
          />
          <MetricsCard
            title="Average Latency"
            value="0.7ms"
            subtitle="end-to-end processing"
            trend="down"
            color="blue"
          />
          <MetricsCard
            title="Queue Depth"
            value="2.1K"
            subtitle="pending messages"
            trend="neutral"
            color="amber"
          />
          <MetricsCard
            title="Memory Usage"
            value="1.2GB"
            subtitle="shared memory pool"
            trend="neutral"
            color="blue"
          />
        </div>

        {/* Charts Row */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          <RealtimeChart
            title="Message Throughput"
            color="#10B981"
            maxValue={150000}
            unit=" msg/s"
          />
          <RealtimeChart
            title="Processing Latency"
            color="#3B82F6"
            maxValue={5}
            unit="ms"
          />
          <RealtimeChart
            title="CPU Utilization"
            color="#F59E0B"
            maxValue={100}
            unit="%"
          />
        </div>

        {/* Control and Detection Row */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          <ControlPanel />
          <MicroburstDetector />
        </div>

        {/* Technical Specifications */}
        <TechnicalSpecs />
      </main>
    </div>
  );
}

export default App;