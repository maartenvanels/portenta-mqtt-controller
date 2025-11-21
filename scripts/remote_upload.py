import sys
import os
import re
import requests
import time

def get_credentials(project_dir):
    cred_file = os.path.join(project_dir, "include", "credentials.h")
    username = None
    password = None
    
    if not os.path.exists(cred_file):
        print(f"Error: Credentials file not found at {cred_file}")
        return None, None

    try:
        with open(cred_file, "r") as f:
            content = f.read()
            
            # Regex to find #define WEB_USERNAME "value"
            user_match = re.search(r'#define\s+WEB_USERNAME\s+"([^"]+)"', content)
            if user_match:
                username = user_match.group(1)
                
            # Regex to find #define WEB_PASSWORD "value"
            pass_match = re.search(r'#define\s+WEB_PASSWORD\s+"([^"]+)"', content)
            if pass_match:
                password = pass_match.group(1)
                
    except Exception as e:
        print(f"Error reading credentials: {e}")
        return None, None

    return username, password

def upload_firmware(source, target, env):
    firmware_path = str(source[0])
    upload_port = env.get("UPLOAD_PORT")
    project_dir = env.get("PROJECT_DIR")

    if not upload_port:
        print("Error: Please specify upload_port (IP address) in platformio.ini or via command line")
        return 1

    print(f"--- Remote OTA Upload ---")
    print(f"Firmware: {firmware_path}")
    print(f"Target IP: {upload_port}")

    # Get credentials
    username, password = get_credentials(project_dir)
    if not username or not password:
        print("Error: Could not find WEB_USERNAME or WEB_PASSWORD in include/credentials.h")
        return 1
    
    print(f"Authenticating as user: {username}")

    base_url = f"http://{upload_port}"
    
    try:
        # 1. Login
        login_url = f"{base_url}/api/login"
        login_data = {"username": username, "password": password}
        
        print(f"Logging in to {login_url}...")
        response = requests.post(login_url, json=login_data, timeout=5)
        
        if response.status_code != 200:
            print(f"Login failed! Status: {response.status_code}")
            print(f"Response: {response.text}")
            return 1
            
        token = response.json().get("token")
        if not token:
            print("Login failed: No token received")
            return 1
            
        print("Login successful, token received")

        # 2. Upload
        upload_url = f"{base_url}/api/upload-firmware"
        headers = {
            "Authorization": f"Bearer {token}"
        }
        
        file_size = os.path.getsize(firmware_path)
        print(f"Uploading firmware ({file_size} bytes)...")
        
        with open(firmware_path, "rb") as f:
            # Stream the upload
            response = requests.post(upload_url, headers=headers, data=f, timeout=60)
            
        if response.status_code == 200:
            print("\nSUCCESS: Firmware uploaded successfully!")
            print(f"Server response: {response.text}")
            print("Device should be rebooting now.")
            return 0
        else:
            print(f"\nUpload failed! Status: {response.status_code}")
            print(f"Response: {response.text}")
            return 1

    except requests.exceptions.ConnectionError:
        print(f"Error: Could not connect to {base_url}. Is the device online?")
        return 1
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        return 1

# PlatformIO hook
try:
    Import("env")
    env.Replace(UPLOADCMD=upload_firmware)
except (ImportError, NameError):
    # Standalone usage (when run via upload_command or manually)
    if __name__ == "__main__":
        if len(sys.argv) < 3:
            print("Usage: python remote_upload.py <firmware_path> <ip_address>")
            sys.exit(1)
        
        firmware_path = sys.argv[1]
        upload_port = sys.argv[2]
        
        # Infer project directory (parent of scripts/)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_dir = os.path.dirname(script_dir)
        
        # Mock env for standalone execution
        class MockEnv:
            def get(self, key):
                if key == "UPLOAD_PORT": return upload_port
                if key == "PROJECT_DIR": return project_dir
                return None
        
        sys.exit(upload_firmware([firmware_path], None, MockEnv()))
