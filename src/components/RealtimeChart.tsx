import React, { useState, useEffect } from 'react';

interface DataPoint {
  time: string;
  value: number;
}

interface RealtimeChartProps {
  title: string;
  color: string;
  maxValue?: number;
  unit?: string;
}

export const RealtimeChart: React.FC<RealtimeChartProps> = ({ 
  title, 
  color, 
  maxValue = 100000,
  unit = ''
}) => {
  const [data, setData] = useState<DataPoint[]>([]);

  useEffect(() => {
    const interval = setInterval(() => {
      const now = new Date();
      const timeStr = now.toLocaleTimeString();
      const value = Math.floor(Math.random() * maxValue * 0.8) + maxValue * 0.2;
      
      setData(prev => {
        const newData = [...prev, { time: timeStr, value }];
        return newData.slice(-20); // Keep last 20 points
      });
    }, 1000);

    return () => clearInterval(interval);
  }, [maxValue]);

  const maxDataValue = Math.max(...data.map(d => d.value), maxValue);

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 p-4">
      <h3 className="text-white font-medium mb-4">{title}</h3>
      <div className="h-32 relative">
        <svg className="w-full h-full" viewBox="0 0 400 120">
          {/* Grid lines */}
          {[0, 1, 2, 3, 4].map(i => (
            <line
              key={i}
              x1="0"
              y1={i * 30}
              x2="400"
              y2={i * 30}
              stroke="#374151"
              strokeWidth="0.5"
              opacity="0.5"
            />
          ))}
          
          {/* Data line */}
          {data.length > 1 && (
            <polyline
              fill="none"
              stroke={color}
              strokeWidth="2"
              points={data
                .map((point, index) => {
                  const x = (index / (data.length - 1)) * 400;
                  const y = 120 - (point.value / maxDataValue) * 120;
                  return `${x},${y}`;
                })
                .join(' ')}
            />
          )}
          
          {/* Data points */}
          {data.map((point, index) => {
            const x = (index / Math.max(data.length - 1, 1)) * 400;
            const y = 120 - (point.value / maxDataValue) * 120;
            return (
              <circle
                key={index}
                cx={x}
                cy={y}
                r="2"
                fill={color}
                opacity={index === data.length - 1 ? 1 : 0.6}
              />
            );
          })}
        </svg>
        
        {/* Current value overlay */}
        {data.length > 0 && (
          <div className="absolute top-2 right-2 bg-gray-900 px-2 py-1 rounded text-sm font-mono" style={{ color }}>
            {data[data.length - 1].value.toLocaleString()}{unit}
          </div>
        )}
      </div>
    </div>
  );
};