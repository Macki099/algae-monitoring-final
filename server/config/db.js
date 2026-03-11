const mongoose = require('mongoose');

const connectDB = async (retryCount = 0) => {
  const uri = process.env.MONGODB_URI;

  if (!uri) {
    console.error('MONGODB_URI is not set. Set it in Railway environment variables.');
    console.error('Retrying in 10 seconds...');
    setTimeout(() => connectDB(retryCount + 1), 10000);
    return;
  }

  try {
    const conn = await mongoose.connect(uri, { serverSelectionTimeoutMS: 10000 });
    console.log(`MongoDB Connected: ${conn.connection.host}`);
  } catch (error) {
    const delay = Math.min(30000, 5000 * (retryCount + 1));
    console.error(`MongoDB connection failed: ${error.message}`);
    console.error(`Retrying in ${delay / 1000}s... (attempt ${retryCount + 1})`);
    setTimeout(() => connectDB(retryCount + 1), delay);
  }
};

module.exports = connectDB;
