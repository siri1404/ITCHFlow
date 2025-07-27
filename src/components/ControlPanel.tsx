import React, { useState } from 'react';
import { Play, Pause, RotateCcw, Settings } from 'lucide-react';

export const ControlPanel: React.FC = () => {
  const [isPlaying, setIsPlaying] = useState(true);
  const [replaySpeed, setReplaySpeed] = useState(1.0);
  const [throttleRate, setThrottleRate] = useState(100000);

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 p-6">
      <div className="flex items-center space-x-2 mb-6">
        <Settings className="h-5 w-5 text-gray-400" />
        <h3 className="text-white font-medium">Control Panel</h3>
      </div>
      
      <div className="space-y-6">
        {/* Playback Controls */}
        <div>
          <h4 className="text-gray-300 text-sm font-medium mb-3">Playback Controls</h4>
          <div className="flex items-center space-x-3">
            <button
              onClick={() => setIsPlaying(!isPlaying)}
              className={`flex items-center space-x-2 px-4 py-2 rounded-lg font-medium transition-colors ${
                isPlaying 
                  ? 'bg-red-600 hover:bg-red-700 text-white' 
                  : 'bg-green-600 hover:bg-green-700 text-white'
              }`}
            >
              {isPlaying ? <Pause className="h-4 w-4" /> : <Play className="h-4 w-4" />}
              <span>{isPlaying ? 'Pause' : 'Play'}</span>
            </button>
            
            <button className="flex items-center space-x-2 px-4 py-2 rounded-lg bg-gray-700 hover:bg-gray-600 text-white font-medium transition-colors">
              <RotateCcw className="h-4 w-4" />
              <span>Reset</span>
            </button>
          </div>
        </div>

        {/* Replay Speed */}
        <div>
          <h4 className="text-gray-300 text-sm font-medium mb-3">Replay Speed</h4>
          <div className="space-y-3">
            <div className="flex items-center justify-between">
              <span className="text-gray-400 text-sm">Speed Multiplier</span>
              <span className="text-blue-400 font-mono">{replaySpeed}x</span>
            </div>
            <input
              type="range"
              min="0.1"
              max="10"
              step="0.1"
              value={replaySpeed}
              onChange={(e) => setReplaySpeed(parseFloat(e.target.value))}
              className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer slider"
            />
            <div className="flex justify-between text-xs text-gray-500">
              <span>0.1x</span>
              <span>1x</span>
              <span>10x</span>
            </div>
          </div>
        </div>

        {/* Throttle Rate */}
        <div>
          <h4 className="text-gray-300 text-sm font-medium mb-3">Throttle Rate</h4>
          <div className="space-y-3">
            <div className="flex items-center justify-between">
              <span className="text-gray-400 text-sm">Messages/sec</span>
              <span className="text-green-400 font-mono">{throttleRate.toLocaleString()}</span>
            </div>
            <input
              type="range"
              min="1000"
              max="500000"
              step="1000"
              value={throttleRate}
              onChange={(e) => setThrottleRate(parseInt(e.target.value))}
              className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer slider"
            />
            <div className="flex justify-between text-xs text-gray-500">
              <span>1K</span>
              <span>100K</span>
              <span>500K</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};