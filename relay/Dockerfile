FROM python:3.11-slim

WORKDIR /app

COPY relay_server.py .

EXPOSE 9000

ENV PYTHONUNBUFFERED=1

CMD ["python3", "relay_server.py", "--host", "0.0.0.0", "--port", "9000"]
