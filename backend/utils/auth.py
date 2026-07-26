"""
Authentication Utilities
Password hashing and credential verification
"""

import hashlib
import json
import os
from pathlib import Path

# Data directory
DATA_DIR = Path(__file__).parent.parent / "data"
DATA_DIR.mkdir(exist_ok=True)

GUARDIANS_FILE = DATA_DIR / "guardians.json"
ELDERLY_FILE = DATA_DIR / "elderly.json"


def hash_password(password):
    """Hash password using SHA-256"""
    return hashlib.sha256(password.encode()).hexdigest()


def verify_password(stored_hash, provided_password):
    """Verify password against stored hash"""
    return stored_hash == hash_password(provided_password)


def load_guardians():
    """Load guardians from JSON file"""
    try:
        if GUARDIANS_FILE.exists():
            with open(GUARDIANS_FILE, 'r') as f:
                content = f.read().strip()
                if content:
                    return json.loads(content)
                else:
                    return {}
        return {}
    except Exception as e:
        print(f"Error loading guardians: {e}")
        return {}


def save_guardians(guardians):
    """Save guardians to JSON file"""
    with open(GUARDIANS_FILE, 'w') as f:
        json.dump(guardians, f, indent=2)


def load_elderly():
    """Load elderly from JSON file"""
    try:
        if ELDERLY_FILE.exists():
            with open(ELDERLY_FILE, 'r') as f:
                content = f.read().strip()
                if content:
                    return json.loads(content)
                else:
                    return {}
        return {}
    except Exception as e:
        print(f"Error loading elderly: {e}")
        return {}


def save_elderly(elderly):
    """Save elderly to JSON file"""
    with open(ELDERLY_FILE, 'w') as f:
        json.dump(elderly, f, indent=2)


def guardian_exists(username):
    """Check if guardian username exists"""
    guardians = load_guardians()
    return username in guardians


def elderly_exists(elderly_id):
    """Check if elderly ID exists"""
    elderly = load_elderly()
    return elderly_id in elderly


def get_guardian(username):
    """Get guardian by username"""
    guardians = load_guardians()
    return guardians.get(username)


def get_elderly(elderly_id):
    """Get elderly by ID"""
    elderly = load_elderly()
    return elderly.get(elderly_id)
