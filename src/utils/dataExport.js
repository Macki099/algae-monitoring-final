import * as XLSX from 'xlsx';

const API_URL = process.env.REACT_APP_API_URL || 'http://localhost:5000/api/sensor-data';

// Export data with date range filter
export const exportDataByDateRange = async ({ startDate, endDate, format = 'json' }) => {
  try {
    // Convert dates to UTC+8 timezone and include full day range
    const start = new Date(startDate + 'T00:00:00+08:00'); // Start of day in UTC+8
    const end = new Date(endDate + 'T23:59:59+08:00'); // End of day in UTC+8
    
    // Fetch data from backend with date range filter
    const params = new URLSearchParams({
      startDate: start.toISOString(),
      endDate: end.toISOString(),
      limit: 10000 // High limit to get all data in range
    });

    const response = await fetch(`${API_URL}?${params}`);
    
    if (!response.ok) {
      throw new Error('Failed to fetch data from server');
    }

    const data = await response.json();

    if (data.length === 0) {
      alert('No data found for the selected date range');
      return;
    }

    // Convert timestamps to readable format (UTC+8)
    const formattedData = data.map(record => ({
      ...record,
      timestamp: new Date(record.timestamp).toLocaleString('en-US', {
        timeZone: 'Asia/Singapore',
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false
      }),
      createdAt: record.createdAt ? new Date(record.createdAt).toLocaleString('en-US', {
        timeZone: 'Asia/Singapore',
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false
      }) : undefined,
      updatedAt: record.updatedAt ? new Date(record.updatedAt).toLocaleString('en-US', {
        timeZone: 'Asia/Singapore',
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false
      }) : undefined
    }));

    // Prepare export data
    const exportData = {
      exportInfo: {
        exportedAt: new Date().toLocaleString('en-US', {
          timeZone: 'Asia/Singapore',
          year: 'numeric',
          month: '2-digit',
          day: '2-digit',
          hour: '2-digit',
          minute: '2-digit',
          second: '2-digit',
          hour12: false
        }),
        startDate,
        endDate,
        totalRecords: formattedData.length,
        format,
        version: '1.0.0',
        timezone: 'UTC+8 (Asia/Singapore)'
      },
      data: formattedData
    };

    if (format === 'json') {
      exportAsJSON(exportData, startDate, endDate);
    } else if (format === 'csv') {
      exportAsCSV(formattedData, startDate, endDate);
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

  // CSV headers
  const headers = [
    'Timestamp',
    'Temperature (°C)',
    'Dissolved Oxygen (mg/L)',
    'pH Level',
    'Electrical Conductivity (µS/cm)',
    'Turbidity (NTU)',
    'Risk Level'
  ];

  // Build CSV content
  let csvContent = headers.join(',') + '\n';

  data.forEach(row => {
    // Format timestamp in UTC+8 timezone
    const timestamp = new Date(row.timestamp).toLocaleString('en-US', {
      timeZone: 'Asia/Singapore',
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
    });
    
    const values = [
      timestamp,
      row.temperature,
      row.dissolvedOxygen,
      row.ph,
      row.electricalConductivity,
      row.turbidity,
      row.riskLevel
    ];
    csvContent += values.join(',') + '\n';
  });

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