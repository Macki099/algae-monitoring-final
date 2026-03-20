import * as XLSX from 'xlsx';
import JSZip from 'jszip';

const RAW_API_URL = process.env.REACT_APP_API_URL || 'http://localhost:5000/api/sensor-data';
const normalizeSensorDataUrl = (url) => {
  let base = (url || '').trim();
  if (base.endsWith('/')) base = base.slice(0, -1);
  if (base.endsWith('/sensor-data')) return base;
  if (base.endsWith('/api')) return `${base}/sensor-data`;
  return `${base}/api/sensor-data`;
};
const API_URL = normalizeSensorDataUrl(RAW_API_URL);

const formatUtc8Timestamp = (value) => new Date(value).toLocaleString('en-US', {
  timeZone: 'Asia/Singapore',
  year: 'numeric',
  month: '2-digit',
  day: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  hour12: false
});

const normalizeRecord = (record) => ({
  ...record,
  electricalConductivity: record.electricalConductivity ?? record.ec ?? 0,
  riskLevel: record.riskLevel ?? record.risk ?? 'unknown'
});

const resolveRangeDate = (value, isEnd = false) => {
  if (!value) {
    return null;
  }

  const hasTime = value.includes('T');
  const dateStr = hasTime
    ? `${value}+08:00`
    : `${value}${isEnd ? 'T23:59:59' : 'T00:00:00'}+08:00`;

  const date = new Date(dateStr);
  return Number.isNaN(date.getTime()) ? null : date;
};

const buildCsvContent = (data) => {
  if (!data.length) return '';

  const headers = [
    'Timestamp',
    'Temperature (°C)',
    'Dissolved Oxygen (mg/L)',
    'pH Level',
    'Electrical Conductivity (µS/cm)',
    'Turbidity (NTU)',
    'Risk Level'
  ];

  const lines = data.map((row) => {
    const values = [
      formatUtc8Timestamp(row.timestamp),
      row.temperature,
      row.dissolvedOxygen,
      row.ph,
      row.electricalConductivity,
      row.turbidity,
      row.riskLevel
    ];

    return values.join(',');
  });

  return `${headers.join(',')}\n${lines.join('\n')}\n`;
};

const extractRiskEvents = (records = []) => records
  .map((record) => {
    const normalizedRisk = String(record.riskLevel || record.risk || 'unknown').toLowerCase();
    const isAlert = Boolean(record.action?.alert);
    const isRiskEvent = isAlert || normalizedRisk === 'moderate' || normalizedRisk === 'high';

    if (!isRiskEvent) {
      return null;
    }

    return {
      timestamp: record.timestamp,
      riskLevel: normalizedRisk,
      confidence: record.riskConfidence ?? null,
      actionReason: record.action?.reason || null,
      alert: isAlert,
      deviceId: record.deviceId || null
    };
  })
  .filter(Boolean);

const buildRiskEventsCsv = (events = []) => {
  const headers = ['Timestamp', 'Risk Level', 'Confidence', 'Alert', 'Action Reason', 'Device ID'];
  const lines = events.map((event) => [
    formatUtc8Timestamp(event.timestamp),
    event.riskLevel,
    event.confidence ?? '',
    event.alert ? 'Yes' : 'No',
    (event.actionReason || '').replaceAll(',', ';'),
    event.deviceId || ''
  ].join(','));

  return `${headers.join(',')}\n${lines.join('\n')}\n`;
};

const getIncludedFiles = (format) => {
  switch (format) {
    case 'bundle':
      return [
        'metadata/export-info.json',
        'metadata/dashboard-context.json',
        'data/sensor-data.json',
        'data/sensor-data.csv',
        'events/risk-events.json',
        'events/risk-events.csv',
        'charts/*.png'
      ];
    case 'excel':
      return ['sensor-data.xlsx'];
    case 'csv':
      return ['sensor-data.csv'];
    case 'json':
    default:
      return ['sensor-data.json'];
  }
};

const slugify = (value) => String(value || 'chart')
  .toLowerCase()
  .replace(/[^a-z0-9]+/g, '-')
  .replace(/(^-|-$)/g, '') || 'chart';

const captureDashboardSnapshots = () => {
  const canvases = Array.from(document.querySelectorAll('.parameter-card canvas, .risk-assessment-card canvas'));

  return canvases
    .map((canvas, index) => {
      try {
        const card = canvas.closest('.parameter-card, .risk-assessment-card');
        const title = card?.querySelector('h3')?.textContent?.trim() || `chart-${index + 1}`;
        const fileName = `charts/${slugify(title)}-${index + 1}.png`;
        const dataUrl = canvas.toDataURL('image/png');
        return { fileName, dataUrl };
      } catch {
        return null;
      }
    })
    .filter(Boolean);
};

const downloadBlob = (blob, fileName) => {
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = fileName;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
};

const exportAsBundle = async ({ records, startDate, endDate, context }) => {
  const zip = new JSZip();
  const filePrefix = `phycosense-report-${startDate}_to_${endDate}`;
  const riskEvents = extractRiskEvents(records);
  const snapshots = captureDashboardSnapshots();

  const exportInfo = {
    exportedAt: formatUtc8Timestamp(new Date().toISOString()),
    startDate,
    endDate,
    totalRecords: records.length,
    riskEventCount: riskEvents.length,
    snapshotCount: snapshots.length,
    timezone: 'UTC+8 (Asia/Singapore)',
    format: 'bundle'
  };

  zip.file('metadata/export-info.json', JSON.stringify(exportInfo, null, 2));
  zip.file('metadata/dashboard-context.json', JSON.stringify(context || {}, null, 2));
  zip.file('data/sensor-data.json', JSON.stringify(records, null, 2));
  zip.file('data/sensor-data.csv', buildCsvContent(records));
  zip.file('events/risk-events.json', JSON.stringify(riskEvents, null, 2));
  zip.file('events/risk-events.csv', buildRiskEventsCsv(riskEvents));

  snapshots.forEach(({ fileName, dataUrl }) => {
    const base64 = dataUrl.split(',')[1];
    zip.file(fileName, base64, { base64: true });
  });

  const blob = await zip.generateAsync({ type: 'blob' });
  downloadBlob(blob, `${filePrefix}.zip`);
};

export const getExportPreview = async ({ startDate, endDate, format = 'json', deviceId }) => {
  const start = resolveRangeDate(startDate, false);
  const end = resolveRangeDate(endDate, true);

  if (!start || !end) {
    throw new Error('Invalid date range selected');
  }

  const params = new URLSearchParams({
    startDate: start.toISOString(),
    endDate: end.toISOString(),
    limit: 10000
  });

  if (deviceId) {
    params.set('deviceId', deviceId);
  }

  const response = await fetch(`${API_URL}?${params}`);

  if (!response.ok) {
    throw new Error('Failed to fetch preview data from server');
  }

  const rawData = await response.json();
  const records = rawData.map(normalizeRecord);

  return {
    estimatedRecords: records.length,
    includedFiles: getIncludedFiles(format),
    riskEventCount: extractRiskEvents(records).length,
    snapshotCount: format === 'bundle' ? captureDashboardSnapshots().length : 0
  };
};

// Export data with date range filter
export const exportDataByDateRange = async ({ startDate, endDate, format = 'json', deviceId, context = {} }) => {
  try {
    const start = resolveRangeDate(startDate, false);
    const end = resolveRangeDate(endDate, true);

    if (!start || !end) {
      throw new Error('Invalid date range selected');
    }
    
    // Fetch data from backend with date range filter
    const params = new URLSearchParams({
      startDate: start.toISOString(),
      endDate: end.toISOString(),
      limit: 10000 // High limit to get all data in range
    });

    if (deviceId) {
      params.set('deviceId', deviceId);
    }

    const response = await fetch(`${API_URL}?${params}`);
    
    if (!response.ok) {
      throw new Error('Failed to fetch data from server');
    }

    const rawData = await response.json();
    const data = rawData.map(normalizeRecord);

    if (data.length === 0) {
      alert('No data found for the selected date range');
      return;
    }

    // Convert timestamps to readable format (UTC+8)
    const formattedData = data.map(record => ({
      ...record,
      timestamp: formatUtc8Timestamp(record.timestamp),
      createdAt: record.createdAt ? formatUtc8Timestamp(record.createdAt) : undefined,
      updatedAt: record.updatedAt ? formatUtc8Timestamp(record.updatedAt) : undefined
    }));

    // Prepare export data
    const exportData = {
      exportInfo: {
        exportedAt: formatUtc8Timestamp(new Date().toISOString()),
        startDate,
        endDate,
        totalRecords: formattedData.length,
        format,
        version: '1.0.0',
        timezone: 'UTC+8 (Asia/Singapore)'
      },
      data: formattedData
    };

    if (format === 'bundle') {
      await exportAsBundle({
        records: data,
        startDate,
        endDate,
        context: {
          ...context,
          exportInfo
        }
      });
    } else if (format === 'json') {
      exportAsJSON(exportData, startDate, endDate);
    } else if (format === 'csv') {
      exportAsCSV(data, startDate, endDate);
    } else if (format === 'excel') {
      exportAsExcel(formattedData, startDate, endDate);
    }

  } catch (error) {
    console.error('Error exporting data:', error);
    alert('Failed to export data. Please ensure the backend server is running.');
  }
};

// Export as JSON
const exportAsJSON = (data, startDate, endDate) => {
  const dataStr = JSON.stringify(data, null, 2);
  const dataBlob = new Blob([dataStr], { type: 'application/json' });
  
  const link = document.createElement('a');
  link.href = URL.createObjectURL(dataBlob);
  link.download = `phycosense-data-${startDate}_to_${endDate}.json`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(link.href);
};

// Export as CSV
const exportAsCSV = (data, startDate, endDate) => {
  if (data.length === 0) return;
  const csvContent = buildCsvContent(data);

  // Download CSV
  const csvBlob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const link = document.createElement('a');
  link.href = URL.createObjectURL(csvBlob);
  link.download = `phycosense-data-${startDate}_to_${endDate}.csv`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(link.href);
};

// Legacy export function (for current readings only)
export const exportData = (sensorData, dataHistory, riskLevels) => {
  const exportData = {
    timestamp: new Date().toISOString(),
    currentReadings: sensorData,
    historicalData: dataHistory,
    riskAssessment: riskLevels,
    systemInfo: {
      version: '1.0.0',
      exportFormat: 'JSON',
      totalDataPoints: Object.values(dataHistory)[0]?.length || 0
    }
  };

  const dataStr = JSON.stringify(exportData, null, 2);
  const dataBlob = new Blob([dataStr], { type: 'application/json' });
  
  const link = document.createElement('a');
  link.href = URL.createObjectURL(dataBlob);
  link.download = `phycosense-data-${new Date().toISOString().split('T')[0]}.json`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  
  // Clean up the URL object
  URL.revokeObjectURL(link.href);
};

// Export as Excel (.xlsx)
const exportAsExcel = (data, startDate, endDate) => {
  if (data.length === 0) return;

  const headers = [
    'Timestamp',
    'Temperature (°C)',
    'Dissolved Oxygen (mg/L)',
    'pH Level',
    'Electrical Conductivity (µS/cm)',
    'Turbidity (NTU)',
    'Risk Level'
  ];

  // Prepare rows (ensure timestamps already formatted to UTC+8)
  const rows = data.map(row => [
    row.timestamp,
    row.temperature,
    row.dissolvedOxygen,
    row.ph,
    row.electricalConductivity,
    row.turbidity,
    row.riskLevel
  ]);

  // Create Excel workbook using SheetJS
  const filename = `phycosense-data-${startDate}_to_${endDate}.xlsx`;

  try {
    // Create workbook and worksheet
    const wb = XLSX.utils.book_new();
    const wsData = [headers, ...rows];
    const ws = XLSX.utils.aoa_to_sheet(wsData);

    // Set column widths for better readability
    ws['!cols'] = [
      { wch: 20 }, // Timestamp
      { wch: 15 }, // Temperature
      { wch: 20 }, // Dissolved Oxygen
      { wch: 12 }, // pH Level
      { wch: 25 }, // Electrical Conductivity
      { wch: 15 }, // Turbidity
      { wch: 12 }  // Risk Level
    ];

    // Add worksheet to workbook
    XLSX.utils.book_append_sheet(wb, ws, 'Sensor Data');

    // Generate Excel file and download
    const wbout = XLSX.write(wb, { bookType: 'xlsx', type: 'array' });
    const blob = new Blob([wbout], { type: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(link.href);
  } catch (error) {
    console.error('Excel export failed:', error);
    alert('Failed to export Excel file. Error: ' + error.message);
  }
};