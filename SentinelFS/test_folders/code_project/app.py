#!/usr/bin/env python3
"""
Sample Python Application
A simple web API for demonstration purposes.
"""

from flask import Flask, jsonify, request
from datetime import datetime
import hashlib
import os

app = Flask(__name__)

# In-memory database
users = {}
sessions = {}

def generate_token(user_id: str) -> str:
    """Generate a session token."""
    data = f"{user_id}{datetime.now().isoformat()}{os.urandom(16).hex()}"
    return hashlib.sha256(data.encode()).hexdigest()[:32]

@app.route('/api/health', methods=['GET'])
def health_check():
    """Health check endpoint."""
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now().isoformat(),
        'version': '1.0.0'
    })

@app.route('/api/users', methods=['POST'])
def create_user():
    """Create a new user."""
    data = request.json
    user_id = hashlib.md5(data['email'].encode()).hexdigest()[:8]
    users[user_id] = {
        'id': user_id,
        'name': data.get('name'),
        'email': data['email'],
        'created_at': datetime.now().isoformat()
    }
    return jsonify(users[user_id]), 201

@app.route('/api/users/<user_id>', methods=['GET'])
def get_user(user_id: str):
    """Get user by ID."""
    if user_id not in users:
        return jsonify({'error': 'User not found'}), 404
    return jsonify(users[user_id])

@app.route('/api/login', methods=['POST'])
def login():
    """User login endpoint."""
    data = request.json
    email = data.get('email')
    # Simple auth for demo
    for uid, user in users.items():
        if user['email'] == email:
            token = generate_token(uid)
            sessions[token] = uid
            return jsonify({'token': token, 'user_id': uid})
    return jsonify({'error': 'Invalid credentials'}), 401

if __name__ == '__main__':
    app.run(debug=True, port=5000)
