import os

projDirPath  = '.'
dataDirPath  = projDirPath + '\\data'
timeFilePath = projDirPath + '\\spiffs-timesum.txt'
file_name1 = 'scripts.js'
file_name2 = 'styles.css'

if(not os.path.exists(timeFilePath)):
  timeFile = open(timeFilePath, "w")
  timeFile.write("0.0")
  timeFile.close()

timeFile = open(timeFilePath, "r")
oldTime = float(timeFile.read())
timeFile.close()
# print(oldTime)

newTime = float(os.path.getmtime(os.path.join(dataDirPath, file_name1)))
newTime = newTime + float(os.path.getmtime(os.path.join(dataDirPath, file_name2)))
# print(newTime)

if newTime != oldTime:
  print("\nfile updated in data directory -- building resources\n")
  os.system(projDirPath + '\\make_c.cmd')

timeFile = open(timeFilePath, "w")
timeFile.write(str(newTime))
timeFile.close()