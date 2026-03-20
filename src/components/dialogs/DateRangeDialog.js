import React, { useState } from 'react';
import '../../styles/Modal.css';

const DateRangeDialog = ({ isOpen, onClose, onExport, onPreview, exportFormat = 'csv' }) => {
  const [startDate, setStartDate] = useState('');
  const [endDate, setEndDate] = useState('');
  const [preview, setPreview] = useState(null);
  const [isLoadingPreview, setIsLoadingPreview] = useState(false);
  const [previewError, setPreviewError] = useState('');

  const resetPreview = () => {
    setPreview(null);
    setPreviewError('');
  };

  const handlePreview = async () => {
    if (!startDate || !endDate) {
      alert('Please select both start and end dates');
      return;
    }

    if (!onPreview) {
      return;
    }

    try {
      setIsLoadingPreview(true);
      setPreviewError('');
      const result = await onPreview({ startDate, endDate });
      setPreview(result);
    } catch (error) {
      setPreview(null);
      setPreviewError(error.message || 'Failed to generate export preview');
    } finally {
      setIsLoadingPreview(false);
    }
  };

  const handleExport = () => {
    if (startDate && endDate) {
      onExport({ startDate, endDate });
      resetPreview();
      onClose();
    } else {
      alert('Please select both start and end dates');
    }
  };

  if (!isOpen) return null;

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h2>Export Data by Date Range</h2>
          <button className="modal-close" onClick={onClose}>×</button>
        </div>
        
        <div className="modal-body">
          <p className="settings-muted" style={{ marginTop: 0 }}>Export format: {String(exportFormat).toUpperCase()}</p>

          <div className="date-input-group">
            <label htmlFor="start-date">Start Date:</label>
            <input
              id="start-date"
              type="datetime-local"
              value={startDate}
              onChange={(e) => {
                setStartDate(e.target.value);
                resetPreview();
              }}
              className="date-input"
            />
          </div>
          
          <div className="date-input-group">
            <label htmlFor="end-date">End Date:</label>
            <input
              id="end-date"
              type="datetime-local"
              value={endDate}
              onChange={(e) => {
                setEndDate(e.target.value);
                resetPreview();
              }}
              className="date-input"
            />
          </div>

          <div className="export-preview-panel">
            <div className="export-preview-header">
              <h3>Preview</h3>
              <button className="btn-secondary" onClick={handlePreview} disabled={isLoadingPreview}>
                {isLoadingPreview ? 'Loading...' : 'Preview Export'}
              </button>
            </div>

            {previewError && <p className="export-preview-error">{previewError}</p>}

            {preview && (
              <div className="export-preview-body">
                <p><strong>Estimated records:</strong> {preview.estimatedRecords}</p>
                {typeof preview.riskEventCount === 'number' && (
                  <p><strong>Risk events in range:</strong> {preview.riskEventCount}</p>
                )}
                {typeof preview.snapshotCount === 'number' && preview.snapshotCount > 0 && (
                  <p><strong>Chart snapshots to include:</strong> {preview.snapshotCount}</p>
                )}

                <div>
                  <strong>Included files:</strong>
                  <ul className="export-preview-list">
                    {preview.includedFiles.map((file) => (
                      <li key={file}>{file}</li>
                    ))}
                  </ul>
                </div>
              </div>
            )}
          </div>
        </div>
        
        <div className="modal-actions">
          <button className="btn-secondary" onClick={onClose}>Cancel</button>
          <button className="btn-primary" onClick={handleExport}>Export</button>
        </div>
      </div>
    </div>
  );
};

export default DateRangeDialog;
