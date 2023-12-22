#!/bin/env python3

import argparse
import os
from pydrive2.auth import GoogleAuth
from pydrive2.drive import GoogleDrive
#from pydrive2.settings import LoadSettingsFile

def main():
    parser = argparse.ArgumentParser(description="Tool to upload mp4 files to Google Drive")
    parser.add_argument("--file", dest="file", required=True)
    parsedArgs = parser.parse_args()

    # Authorization is a nightmare, here's what worked for me:
    # Visit https://developers.google.com/drive/api/quickstart/python and follow their quickstart.py example
    # Using the client_secrets.json file downloaded from the web console, this generates a refresh token called token.json in the working directory
    # This token will not expire as long as the app is marked as in production instead of in testing (7 day expiration)
    # Now using this refresh token, the PyDrive API can be used seamlessly
    # Note that to fix the redirect_uri_mismatch problems (https://stackoverflow.com/questions/70086449/error-400-redirect-uri-mismatch-trying-to-access-google-drive), 
    # adding http://localhost to the "Authorized redirect URIs" for this client fixed this
    gauth = GoogleAuth()
    # For creating a secrets file, PyDrive settings can be used
    #gauth.settings = LoadSettingsFile(filename="settings.yaml")
    gauth.LocalWebserverAuth()
    # Only works for native apps, not web apps
    #gauth.CommandLineAuth()
    drive = GoogleDrive(gauth)

    parentFolder="1VaQvic_Aa_nYA254GADsrxaieSwxuZg-"
    f = drive.CreateFile({
        "title": os.path.basename(parsedArgs.file),
        "parents": [{
            "id": parentFolder
        }],
        "mimeType": "video/mp4"
    })
    f.SetContentFile(parsedArgs.file)
    f.Upload()

    return 0

if __name__ == "__main__":
    exit(code=main())
