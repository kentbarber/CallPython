# Call Python
A C4D plugin to call python scripts at various points in a pipeline.

## Running Python after a scene is loaded
When a scene is loaded it will call the python script called "afterloadpython.py" that is located directly next to the "callpython.xdl64" file.

The variables doc and op are defined and can be used directly in the python script, where op is the first object in the scene.

## Running a custom python file when C4D starts up
1. Copy the runpython folder to your plugins directory
2. Put the python script you want to run somewhere on your drive
3. Running the following from your command prompt

c:\Program Files\Maxon Cinema 4D R2024>"Cinema 4D.exe" -runpython "F:\createcube.py"

The variables doc and op are defined and can be used directly in the python script, where op is the first object in the scene.

## Included Files
- afterloadpython.py - a simple script that writes a message to the console python log after a scene has been loaded
- createcube.py - a simple script to create a cube in the scene
