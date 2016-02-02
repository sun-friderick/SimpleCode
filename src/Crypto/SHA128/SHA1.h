//! SHA1 ��̬���ӿ�ʵ��   H�ļ�
/*��
 @author Frederick
 @e-mail  zmb.tsubasa@gmail.com
 @version 1.0
 @date 2011-03
 @{
*/
#ifndef __SHA1_H__
#define __SHA1_H__

#include "stdint.h"



//! #����SHA �еķ���ENUM
/*��
   @see enum
*/
#ifndef _SHA_enum_
#define _SHA_enum_
enum
{
	shaSuccess = 0,
	/*! <��ָʾ���� */
	shaNull,            
	/*! < ��������̫����ʾ */
	shaInputTooLong,
	 /*! <called Input after Result --������������֮ */
	shaStateError      
};
#endif  //_SHA_enum_



//! SHA1HashSize�������SHA1��ϣ��Ĵ�С
#define SHA1HashSize 20


//!   #�ܹ����ж�̬���ӿ�����SHA1��

 /*!  
       @see class _declspec(dllexport) SHA_1
       ��SHA1�㷨д�ɶ�̬���ӿ����ʽ������ã�������Ϣʹ��
 */
class  SHA_1  //_declspec(dllexport)
{
public:
	//! #�������ݽṹ������������Ϣ SHA1Context
	/*!
	    �������ֽṹ���������������Ϣ for the SHA-1
	    hashing operation
		@see struct SHA1Context
	*/
	typedef struct SHA1Context
	{
		uint32_t Intermediate_Hash[SHA1HashSize/4]; /*! <Message Digest  */

		uint32_t Length_Low;            /*! <Message length in bits      */
		uint32_t Length_High;           /*! <Message length in bits      */

		/*! <Index into message block array   */
		int_least16_t Message_Block_Index;
		uint8_t Message_Block[64];      /*! <512-bit message blocks      */

		int Computed;               /*! <Is the digest computed?         */
		int Corrupted;             /*! <Is the message digest corrupted? */
	} SHA1Context;

public:
    //! #SHA_1 �Ĺ��캯��
    /*!
	 @see SHA_1()
      ����Ӧ�ö�SHA_1���е�һЩ����������Ӧ�ĳ�ʼ��
    */
	SHA_1();
	//! #SHA_1����������
    /*!
	@see ~SHA_1()
      �ͷ��ڴ�
    */
	~SHA_1(void);

	/*----------------------------------����ԭ��----------------------------------*/
	//! #SHA1�㷨�е��������ģ��
    /*!
	  @see void SHA1PadMessage(SHA1Context *);
	  @param[SHA1Context*  ���������Ϣָ��
	  @return[void] �������κ�ֵ
    */
	void SHA1PadMessage(SHA1Context *);    /*  ���������Ϣָ��  */
    //! #SHA1����Ϣ����������
    /*!
	  @see void SHA1ProcessMessageBlock(SHA1Context *);
	  @param[SHA1Context*  ���������Ϣָ��
	  @param[in] ��Ϣ�鳤��Ϊ�̶�֮512����
	  @return[void] �������κ�ֵ
    */
	void SHA1ProcessMessageBlock(SHA1Context *);
	//! #SHA1�����ݳ�ʼ����������
    /*!
	  @see int SHA1Reset(  SHA1Context *);
	  @param[SHA1Context*  ���������Ϣָ��
	  @return[int] �ɹ�����shaNull��ʧ�ܷ���shaSuccess
	  @see SHA1 enum
    */
	int SHA1Reset(  SHA1Context *);
	//! #SHA1��������������
    /*!
	  @see int SHA1Input(  SHA1Context *, const uint8_t *, unsigned int);
	  @param[SHA1Context*  ���������Ϣָ��
      @param[uint8_t ���յ�λ����Ϊ8�ֽڱ�������Ϣ
	  @return[enum] �ɹ�����shaNull��ʧ�ܷ���shaSuccess�����󷵻�shaStateError
	  @see SHA1 enum
    */
	int SHA1Input(  SHA1Context *, const uint8_t *, unsigned int);
	//! #SHA1�Ľ����������
    /*!
	  @see int SHA1Result( SHA1Context *, uint8_t Message_Digest[SHA1HashSize]);
	  @param[SHA1Context*  ���������Ϣָ��
      @param[uint8_t 160���ص���ϢժҪ����
	  @attention ����һ��160���ص���ϢժҪ����
	  @return[enum] �ɹ�����shaNull��ʧ�ܷ���shaSuccess�����󷵻�shaStateError
	  @see SHA1 enum
    */
	int SHA1Result( SHA1Context *, uint8_t Message_Digest[SHA1HashSize]);

private:
};



#endif // __SHA1_H__
