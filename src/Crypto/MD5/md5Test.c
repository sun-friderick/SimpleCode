#include "md5c.h"

void main(void)
{
    unsigned char digest[16];  //��Ž��

    //��һ���÷�:

    MD5_CTX md5c;
    MD5Init(&md5c); //��ʼ��
    MD5UpdaterString(&md5c, "��Ҫ���Ե��ַ���");
    MD5FileUpdateFile(&md5c, "��Ҫ���Ե��ļ�·��");
    MD5Final(digest, &md5c);

    //�ڶ����÷�:
    MDString("��Ҫ���Ե��ַ���", digest); //ֱ�������ַ������ó����

    //�������÷�:
    MD5File("��Ҫ���Ե��ļ�·��", digest); //ֱ�������ļ�·�����ó����
}

