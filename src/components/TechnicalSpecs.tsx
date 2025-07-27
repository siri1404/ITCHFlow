import React from 'react';
import { Cpu, Database, Network, Zap } from 'lucide-react';

export const TechnicalSpecs: React.FC = () => {
  const specs = [
    {
      icon: Cpu,
      title: 'Processing Engine',
      items: [
        'C++ high-performance core',
        'epoll-based event handling',
        'Lock-free data structures',
        'NUMA-aware memory allocation'
      ]
    },
    {
      icon: Network,
      title: 'Communication',
      items: [
        'ZeroMQ message queuing',
        'TCP/UDP multicast support',
        'Binary protocol optimization',
        'Heartbeat & failover'
      ]
    },
    {
      icon: Database,
      title: 'Data Management',
      items: [
        'Shared memory IPC',
        'Zero-copy message passing',
        'NASDAQ ITCH v5.0 parser',
        'Real-time normalization'
      ]
    },
    {
      icon: Zap,
      title: 'Performance',
      items: [
        'Sub-millisecond latency',
        '100K+ messages/second',
        'Microsecond precision timing',
        'CPU affinity optimization'
      ]
    }
  ];

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 p-6">
      <h3 className="text-white font-medium mb-6">Technical Architecture</h3>
      
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {specs.map((spec, index) => (
          <div key={index} className="space-y-3">
            <div className="flex items-center space-x-2 mb-3">
              <spec.icon className="h-5 w-5 text-blue-400" />
              <h4 className="text-gray-200 font-medium">{spec.title}</h4>
            </div>
            <ul className="space-y-2">
              {spec.items.map((item, itemIndex) => (
                <li key={itemIndex} className="text-gray-400 text-sm flex items-center space-x-2">
                  <div className="w-1.5 h-1.5 bg-blue-400 rounded-full flex-shrink-0" />
                  <span>{item}</span>
                </li>
              ))}
            </ul>
          </div>
        ))}
      </div>
    </div>
  );
};