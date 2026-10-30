import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    SECRET_KEY = os.getenv("SECRET_KEY", "smart-water-leakage-secret-key-2026")
    
    # Database configuration
    # Priority: DATABASE_URL (Render / Cloud) -> local PostgreSQL -> SQLite fallback
    DB_USER = os.getenv("DB_USER", "postgres")
    DB_PASSWORD = os.getenv("DB_PASSWORD", "postgres")
    DB_HOST = os.getenv("DB_HOST", "localhost")
    DB_PORT = os.getenv("DB_PORT", "5432")
    DB_NAME = os.getenv("DB_NAME", "water_leakage_db")
    
    DEFAULT_POSTGRES_URI = f"postgresql://{DB_USER}:{DB_PASSWORD}@{DB_HOST}:{DB_PORT}/{DB_NAME}"
    raw_db_url = os.getenv("DATABASE_URL", DEFAULT_POSTGRES_URI)
    
    # Render PostgreSQL URLs start with postgres://, which SQLAlchemy 1.4+ deprecated in favor of postgresql://
    if raw_db_url and raw_db_url.startswith("postgres://"):
        raw_db_url = raw_db_url.replace("postgres://", "postgresql://", 1)
        
    SQLALCHEMY_DATABASE_URI = raw_db_url
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    
    # Offline timeout: If no reading/heartbeat received within X seconds, mark device offline
    DEVICE_OFFLINE_THRESHOLD_SECONDS = int(os.getenv("DEVICE_OFFLINE_THRESHOLD_SECONDS", 15))
    
    # Server port & host (Render dynamically injects PORT)
    PORT = int(os.getenv("PORT", 5000))
    HOST = os.getenv("HOST", "0.0.0.0")


