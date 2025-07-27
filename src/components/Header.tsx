import React from 'react';
import { Activity, Zap, Database } from 'lucide-react';

export const Header: React.FC = () => {
  return (
    <header className="bg-gray-900 border-b border-gray-700 px-6 py-4">
      <div className="flex items-center justify-between">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2">
            <Zap className="h-8 w-8 text-blue-400" />
            <h1 className="text-2xl font-bold text-white">TickShaper</h1>
          </div>
          <span className="text-gray-400 text-sm">Real-Time Market Data Throttler</span>
        </div>
        
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2">
            <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
            <span className="text-green-400 text-sm font-medium">LIVE</span>
          </div>
          
          <div className="flex items-center space-x-4 text-sm">
            <div className="flex items-center space-x-1">
              <Activity className="h-4 w-4 text-blue-400" />
              <span className="text-gray-300">Uptime: 15d 4h 23m</span>
            </div>
            <div className="flex items-center space-x-1">
              <Database className="h-4 w-4 text-green-400" />
              <span className="text-gray-300">NASDAQ ITCH v5.0</span>
            </div>
          </div>
        </div>
      </div>
    </header>
  );
};