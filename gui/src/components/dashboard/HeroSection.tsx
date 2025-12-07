import { Shield, TrendingUp, Network, Clock } from 'lucide-react'
import { QuickStat } from './QuickStat'

interface HeroSectionProps {
  peakUpload: number
  peakDownload: number
  peersCount: number
  formatBytes: (bytes: number) => string
}

export function HeroSection({ peakUpload, peakDownload, peersCount, formatBytes }: HeroSectionProps) {
  return (
    <div className="hero-section">
      {/* Animated Background Effects */}
      <div className="absolute inset-0 overflow-hidden">
        <div className="hero-bg-gradient-1"></div>
        <div className="hero-bg-gradient-2"></div>
        {/* Grid Pattern Overlay */}
        <div className="hero-grid-pattern"></div>
      </div>
      
      <div className="relative z-10">
        <div className="flex flex-col sm:flex-row sm:items-center gap-3 mb-4">
          <div className="hero-icon-wrapper">
            <Shield className="w-6 h-6 sm:w-8 sm:h-8 text-primary" />
          </div>
          <div>
            <h1 className="hero-title">
              System Overview
            </h1>
            <p className="hero-subtitle">
              <span className="dot-success animate-pulse"></span>
              All systems operational
            </p>
          </div>
        </div>
        
        {/* Quick Stats Row */}
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-2 sm:gap-4 mt-4 sm:mt-6">
          <QuickStat 
            label="Upload Speed" 
            value={formatBytes(peakUpload) + '/s'} 
            icon={<TrendingUp className="w-4 h-4" />}
            color="teal"
          />
          <QuickStat 
            label="Download Speed" 
            value={formatBytes(peakDownload) + '/s'} 
            icon={<TrendingUp className="w-4 h-4" />}
            color="emerald"
          />
          <QuickStat 
            label="Connected Peers" 
            value={peersCount || 0} 
            icon={<Network className="w-4 h-4" />}
            color="coral"
          />
          <QuickStat 
            label="Uptime" 
            value="99.9%" 
            icon={<Clock className="w-4 h-4" />}
            color="amber"
          />
        </div>
      </div>
    </div>
  )
}
