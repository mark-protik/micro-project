

from flask import Flask, request, jsonify
import joblib
import numpy as np
import csv
from datetime import datetime
import os

app = Flask(__name__)

model = joblib.load('best_crop_model.pkl')
label_encoder = joblib.load('label_encoder.pkl')
CSV_FILE = "sensor_data.csv"

# Ensure CSV file has headers
if not os.path.exists(CSV_FILE):
    with open(CSV_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            "Timestamp", "N", "P", "K", "Temperature", "Humidity",
            "pH", "Rainfall", "Moisture_Voltage", "Moisture_Status"
        ])

@app.route('/predict', methods=['POST'])
def predict():
    data = request.get_json()

    features = [
        data.get('N'),
        data.get('P'),
        data.get('K'),
        data.get('temperature'),
        data.get('humidity'),
        data.get('ph'),
        data.get('rainfall')
    ]
    features_array = np.array([features])

    if hasattr(model, "predict_proba"):
        probas = model.predict_proba(features_array)[0]
        top_indices = np.argsort(probas)[::-1][:5]

        best_crop = label_encoder.inverse_transform([top_indices[0]])[0]
        other_suggestions = label_encoder.inverse_transform(top_indices[1:]).tolist()
    else:
        pred_encoded = model.predict(features_array)[0]
        best_crop = label_encoder.inverse_transform([pred_encoded])[0]
        other_suggestions = []

    # === Save to CSV ===
    with open(CSV_FILE, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            data.get('N'),
            data.get('P'),
            data.get('K'),
            data.get('temperature'),
            data.get('humidity'),
            data.get('ph'),
            data.get('rainfall'),
            data.get('moisture'),
            data.get('moisture_status')
        ])

    return jsonify({
        'best_crop': best_crop,
        'other_suggestions': other_suggestions
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
