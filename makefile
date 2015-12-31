
# �R���p�C��
CC = gcc
CFLAGS =-O2 

# ���s�t�@�C����
TARGET = 2shot.cgi
# (�f�t�H���g�́Aparent�f�B���N�g���ɍ쐬����܂�)
# �����t�@�C��

# �Q���҃t�@�C��

DCH_SRC_FILE = config.h ycookie.h yutils.h md5.h
DCH_OBJ_FILE = 2shot.o md5.o ycookie.o yutils.o base64.o

$(TARGET): $(DCH_OBJ_FILE)  $(DCH_SRC_FILE)
	$(CC) -o $(TARGET) $(DCH_OBJ_FILE) 
	chmod 755 $(TARGET)

2chat.o:    $(DCH_SRC_FILE)
md5.o:   $(DCH_SRC_FILE)
ycookie.o:   $(DCH_SRC_FILE)
yutils.o:   $(DCH_SRC_FILE)
kfmem.o:   $(DCH_SRC_FILE)
base64.o:   $(DCH_SRC_FILE)

clean:
	rm -f *.o

del:
	rm -f  *# *~ *.bak *.BAK *.o $(TARGET).core

