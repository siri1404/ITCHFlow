import React from 'react';

interface MetricsCardProps {
  title: string;
  value: string;
  subtitle?: string;
  trend?: 'up' | 'down' | 'neutral';
  color?: 'green' | 'blue' | 'amber' | 'red';
}

export const MetricsCard: React.FC<MetricsCardProps> = ({ 
  title, 
  value, 
  subtitle, 
  trend = 'neutral',
  color = 'blue' 
}) => {
  const colorClasses = {
    green: 'text-green-400 bg-green-900/20 border-green-700',
    blue: 'text-blue-400 bg-blue-900/20 border-blue-700',
    amber: 'text-amber-400 bg-amber-900/20 border-amber-700',
    red: 'text-red-400 bg-red-900/20 border-red-700'
  };

  const trendSymbol = trend === 'up' ? '↗' : trend === 'down' ? '↘' : '';

  return (
    <div className={`rounded-lg border p-4 ${colorClasses[color]}`}>
      <h3 className="text-gray-300 text-sm font-medium mb-2">{title}</h3>
      <div className="flex items-baseline space-x-2">
        <span className="text-2xl font-bold font-mono">{value}</span>
        {trendSymbol && (
          <span className="text-sm opacity-75">{trendSymbol}</span>
        )}
      </div>
      {subtitle && (
        <p className="text-xs opacity-75 mt-1">{subtitle}</p>
      )}
    </div>
  );
};