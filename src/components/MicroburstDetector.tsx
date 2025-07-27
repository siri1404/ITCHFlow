import React, { useState, useEffect } from 'react';
import { AlertTriangle, TrendingUp } from 'lucide-react';

interface MicroburstEvent {
  id: string;
  timestamp: string;
  peakRate: number;
  duration: number;
  severity: 'low' | 'medium' | 'high';
}

export const MicroburstDetector: React.FC = () => {
  const [events, setEvents] = useState<MicroburstEvent[]>([]);
  const [currentBurstRate, setCurrentBurstRate] = useState(0);

  useEffect(() => {
    const interval = setInterval(() => {
      // Simulate microburst detection
      const burstRate = Math.random() * 200000 + 50000;
      setCurrentBurstRate(burstRate);

      // Occasionally generate a microburst event
      if (Math.random() < 0.1) {
        const severity = burstRate > 180000 ? 'high' : burstRate > 120000 ? 'medium' : 'low';
        const newEvent: MicroburstEvent = {
          id: Math.random().toString(36).substr(2, 9),
          timestamp: new Date().toLocaleTimeString(),
          peakRate: Math.floor(burstRate),
          duration: Math.floor(Math.random() * 500) + 100,
          severity
        };

        setEvents(prev => [newEvent, ...prev.slice(0, 4)]);
      }
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  const getSeverityColor = (severity: string) => {
    switch (severity) {
      case 'high': return 'text-red-400 bg-red-900/20 border-red-700';
      case 'medium': return 'text-amber-400 bg-amber-900/20 border-amber-700';
      case 'low': return 'text-green-400 bg-green-900/20 border-green-700';
      default: return 'text-gray-400 bg-gray-900/20 border-gray-700';
    }
  };

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 p-6">
      <div className="flex items-center space-x-2 mb-6">
        <AlertTriangle className="h-5 w-5 text-amber-400" />
        <h3 className="text-white font-medium">Microburst Detection</h3>
      </div>

      {/* Current Rate Monitor */}
      <div className="mb-6 p-4 bg-gray-900 rounded-lg border border-gray-600">
        <div className="flex items-center justify-between mb-2">
          <span className="text-gray-300 text-sm">Current Burst Rate</span>
          <TrendingUp className="h-4 w-4 text-blue-400" />
        </div>
        <div className="text-2xl font-mono text-blue-400 font-bold">
          {currentBurstRate.toLocaleString()} msg/s
        </div>
        <div className="w-full bg-gray-700 rounded-full h-2 mt-2">
          <div 
            className="bg-blue-500 h-2 rounded-full transition-all duration-500"
            style={{ width: `${Math.min((currentBurstRate / 200000) * 100, 100)}%` }}
          />
        </div>
      </div>

      {/* Recent Events */}
      <div>
        <h4 className="text-gray-300 text-sm font-medium mb-3">Recent Events</h4>
        <div className="space-y-2 max-h-48 overflow-y-auto">
          {events.length === 0 ? (
            <div className="text-gray-500 text-sm italic p-2">No recent microburst events</div>
          ) : (
            events.map(event => (
              <div 
                key={event.id}
                className={`p-3 rounded-lg border text-sm ${getSeverityColor(event.severity)}`}
              >
                <div className="flex items-center justify-between mb-1">
                  <span className="font-medium uppercase text-xs">{event.severity} SEVERITY</span>
                  <span className="text-xs opacity-75">{event.timestamp}</span>
                </div>
                <div className="space-y-1">
                  <div>Peak: <span className="font-mono">{event.peakRate.toLocaleString()}</span> msg/s</div>
                  <div>Duration: <span className="font-mono">{event.duration}ms</span></div>
                </div>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
};