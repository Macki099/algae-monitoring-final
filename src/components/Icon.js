import React from 'react';
import { 
  Thermometer, 
  Droplets, 
  FlaskConical, 
  Zap, 
  Eye, 
  Wifi, 
  WifiOff,
  AlertTriangle,
  Shield,
  Download,
  Settings,
  List,
  ChevronDown,
  Activity,
  Info,
  Cpu,
  TrendingUp,
  BarChart3
} from 'lucide-react';

const iconMap = {
  thermometer: Thermometer,
  droplets: Droplets,
  flask: FlaskConical,
  zap: Zap,
  eye: Eye,
  wifi: Wifi,
  wifiOff: WifiOff,
  alertTriangle: AlertTriangle,
  shield: Shield,
  download: Download,
  settings: Settings,
  list: List,
  chevronDown: ChevronDown,
  activity: Activity,
  info: Info,
  cpu: Cpu,
  'trending-up': TrendingUp,
  barChart: BarChart3,
  'alert-triangle': AlertTriangle
};

const Icon = ({ name, size = 24, className = "", ...props }) => {
  const IconComponent = iconMap[name];
  
  if (!IconComponent) {
    console.warn(`Icon "${name}" not found`);
    return null;
  }
  
  return <IconComponent size={size} className={className} {...props} />;
};

export default Icon;