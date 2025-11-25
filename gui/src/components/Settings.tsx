import { useState } from 'react'
import { Bell, Database, Globe, Lock, Moon, Shield, Smartphone } from 'lucide-react'

export function Settings() {
  const [activeTab, setActiveTab] = useState('general')

  return (
    <div className="max-w-4xl mx-auto animate-in fade-in duration-500">
      <h2 className="text-lg font-semibold mb-6">Settings</h2>
      
      <div className="grid grid-cols-1 md:grid-cols-4 gap-8">
        {/* Settings Navigation */}
        <nav className="space-y-1">
            <SettingsTab 
                active={activeTab === 'general'} 
                onClick={() => setActiveTab('general')} 
                icon={<Smartphone className="w-4 h-4" />} 
                label="General" 
            />
            <SettingsTab 
                active={activeTab === 'network'} 
                onClick={() => setActiveTab('network')} 
                icon={<Globe className="w-4 h-4" />} 
                label="Network" 
            />
            <SettingsTab 
                active={activeTab === 'security'} 
                onClick={() => setActiveTab('security')} 
                icon={<Lock className="w-4 h-4" />} 
                label="Security" 
            />
            <SettingsTab 
                active={activeTab === 'advanced'} 
                onClick={() => setActiveTab('advanced')} 
                icon={<Database className="w-4 h-4" />} 
                label="Advanced" 
            />
        </nav>

        {/* Settings Content */}
        <div className="md:col-span-3 space-y-6">
            {activeTab === 'general' && (
                <div className="space-y-6">
                    <Section title="Device Identity">
                        <Input label="Device Name" value="Sentinel-Desktop-Linux" />
                        <div className="text-xs text-muted-foreground mt-2">This name will be visible to other peers.</div>
                    </Section>
                    <Section title="Interface">
                        <div className="flex items-center justify-between">
                            <div className="flex items-center gap-3">
                                <Moon className="w-5 h-5 text-muted-foreground" />
                                <span className="text-sm font-medium">Dark Mode</span>
                            </div>
                            <Toggle checked={true} />
                        </div>
                    </Section>
                </div>
            )}

            {activeTab === 'network' && (
                <div className="space-y-6">
                    <Section title="Connection">
                        <Input label="Listen Port" value="22000" />
                        <div className="flex items-center justify-between mt-4">
                            <span className="text-sm font-medium">Enable UPnP</span>
                            <Toggle checked={true} />
                        </div>
                    </Section>
                    <Section title="Bandwidth Limits">
                        <Input label="Upload Limit (KB/s)" value="0 (Unlimited)" />
                        <Input label="Download Limit (KB/s)" value="0 (Unlimited)" />
                    </Section>
                </div>
            )}
            
            {activeTab === 'security' && (
                <div className="space-y-6">
                    <Section title="Encryption">
                        <div className="bg-secondary/30 p-4 rounded-lg border border-border">
                            <div className="flex items-center gap-2 text-green-500 mb-2">
                                <Shield className="w-4 h-4" />
                                <span className="text-sm font-semibold">AES-256 Encryption Active</span>
                            </div>
                            <p className="text-xs text-muted-foreground">All traffic between peers is end-to-end encrypted using TLS 1.3.</p>
                        </div>
                    </Section>
                    <Section title="ML Security Layer">
                        <div className="flex items-center justify-between mb-4">
                            <div>
                                <span className="text-sm font-medium block">Anomaly Detection</span>
                                <span className="text-xs text-muted-foreground">Block suspicious peers automatically</span>
                            </div>
                            <Toggle checked={false} />
                        </div>
                        <div className="opacity-50 pointer-events-none">
                            <label className="text-xs font-medium mb-1 block">Sensitivity Threshold</label>
                            <input type="range" className="w-full h-2 bg-secondary rounded-lg appearance-none cursor-pointer" />
                        </div>
                    </Section>
                </div>
            )}
        </div>
      </div>
    </div>
  )
}

function SettingsTab({ active, onClick, icon, label }: any) {
    return (
        <button 
            onClick={onClick}
            className={`w-full flex items-center gap-3 px-4 py-2 rounded-lg text-sm font-medium transition-colors ${
                active ? 'bg-secondary text-foreground' : 'text-muted-foreground hover:bg-secondary/50 hover:text-foreground'
            }`}
        >
            {icon}
            {label}
        </button>
    )
}

function Section({ title, children }: any) {
    return (
        <div className="bg-card border border-border rounded-xl p-6 shadow-sm">
            <h3 className="font-medium mb-4 pb-2 border-b border-border">{title}</h3>
            {children}
        </div>
    )
}

function Input({ label, value }: any) {
    return (
        <div className="mb-4 last:mb-0">
            <label className="block text-xs font-medium text-muted-foreground mb-1.5 uppercase tracking-wider">{label}</label>
            <input 
                type="text" 
                defaultValue={value} 
                className="w-full bg-background border border-input rounded-md px-3 py-2 text-sm focus:ring-1 focus:ring-blue-500 outline-none transition-all" 
            />
        </div>
    )
}

function Toggle({ checked }: any) {
    return (
        <div className={`w-10 h-5 rounded-full relative cursor-pointer transition-colors ${checked ? 'bg-blue-600' : 'bg-secondary'}`}>
            <div className={`absolute top-1 w-3 h-3 bg-white rounded-full transition-all ${checked ? 'right-1' : 'left-1'}`}></div>
        </div>
    )
}
