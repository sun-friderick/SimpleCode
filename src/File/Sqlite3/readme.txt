
��������Դ�� ��Դ��� sqlite3

Դ����� ������ sqlite-amalgamation-3081002.zip Դ��� 


ע�⣺
    sqlite3.cԴ�ļ��ھ���astyle��ʽ��֮������


��Ҫ�޸�Դ�룺
    1�� # define SQLITE_DEFAULT_CACHE_SIZE  8000   /** original is 2000 **/


    

�����Ż�:
�ܶ���ֱ�Ӿ�ʹ���ˣ���δע�⵽SQLiteҲ�����ò��������Զ����ܽ��е�������ʱ�򣬲����Ľ�����кܴ�Ӱ�졣
��Ҫͨ��pragmaָ����ʵ�֡�
���磺 �ռ��ͷš�����ͬ����Cache��С�ȡ�
��Ҫ�򿪡�ǰ������ˣ�Vacuum��Ч�ʷǳ��ͣ�

1 auto_vacuum
    PRAGMA auto_vacuum; 
    PRAGMA auto_vacuum = 0 | 1;
    ��ѯ���������ݿ��auto-vacuum��ǡ�
    ��������£����ύһ�������ݿ���ɾ�����ݵ�����ʱ�����ݿ��ļ����ı��С��δʹ�õ��ļ�ҳ����ǲ����Ժ����Ӳ������ٴ�ʹ�á����������ʹ��VACUUM�����ͷ�ɾ���õ��Ŀռ䡣
    ������auto-vacuum�����ύһ�������ݿ���ɾ�����ݵ�����ʱ�����ݿ��ļ��Զ������� (VACUUM������auto-vacuum���������ݿ��в�������)�����ݿ�����ڲ��洢һЩ��Ϣ�Ա�֧����һ���ܣ���ʹ�����ݿ��ļ��Ȳ�������ѡ��ʱ��΢��һЩ��
    ֻ�������ݿ���δ���κα�ʱ���ܸı�auto-vacuum��ǡ���ͼ�����б��������޸Ĳ��ᵼ�±���
    
2 cache_size
    �����Ϊ8000
    PRAGMA cache_size; 
    PRAGMA cache_size = Number-of-pages;
    ��ѯ���޸�SQLiteһ�δ洢���ڴ��е����ݿ��ļ�ҳ����ÿҳʹ��Լ1.5K�ڴ棬ȱʡ�Ļ����С��2000. ����Ҫʹ�øı�������е�UPDATE��DELETE������Ҳ�����SQLiteʹ�ø�����ڴ�Ļ����������󻺴���������ܡ�
    ��ʹ��cache_size pragma�ı仺���Сʱ���ı���Ե�ǰ�Ի���Ч�������ݿ�ر����´�ʱ�����С�ָ���ȱʡ��С�� Ҫ�����øı仺���С��ʹ��default_cache_size pragma.
    
3 case_sensitive_like
    �򿪡���Ȼ���������ִ������
    PRAGMA case_sensitive_like; 
    PRAGMA case_sensitive_like = 0 | 1;
    LIKE�������ȱʡ��Ϊ�Ǻ���latin1�ַ��Ĵ�Сд�������ȱʡ�����'a' LIKE 'A'��ֵΪ�档����ͨ����case_sensitive_like pragma���ı���һȱʡ��Ϊ��������case_sensitive_like��'a' LIKE 'A'Ϊ�ٶ� 'a' LIKE 'a'��ȻΪ�档
    
4 count_changes
    �򿪡����ڵ���
    PRAGMA count_changes; 
    PRAGMA count_changes = 0 | 1;
    ��ѯ�����count-changes��ǡ����������INSERT, UPDATE��DELETE��䲻�������ݡ� ������count-changes��������䷵��һ�к�һ������ֵ�����ݡ����������룬�޸Ļ�ɾ���������� ���ص������������ɴ����������Ĳ��룬�޸Ļ�ɾ���ȸı��������
    
5 page_size
    PRAGMA page_size; 
    PRAGMA page_size = bytes;
    ��ѯ������page-sizeֵ��ֻ����δ�������ݿ�ʱ��������page-size��ҳ���С������2���������Ҵ��ڵ���512С�ڵ���8192�� ���޿���ͨ���ڱ���ʱ�޸ĺ궨��SQLITE_MAX_PAGE_SIZE��ֵ���ı䡣���޵�������32768.
    
6 synchronous
    ����ж��ڱ��ݵĻ��ƣ������������ݶ�ʧ�ɽ��ܣ���OFF
    PRAGMA synchronous; 
    PRAGMA synchronous = FULL; (2) 
    PRAGMA synchronous = NORMAL; (1) 
    PRAGMA synchronous = OFF; (0)
    ��ѯ�����"synchronous"��ǵ��趨����һ����ʽ(��ѯ)��������ֵ�� 
    ��synchronous����ΪFULL (2), SQLite���ݿ������ڽ���ʱ�̻���ͣ��ȷ�������Ѿ�д����̡� ��ʹϵͳ�������Դ������ʱ��ȷ�����ݿ�������󲻻��𻵡�FULL synchronous�ܰ�ȫ�������� 
    ��synchronous����ΪNORMAL, SQLite���ݿ������ڴ󲿷ֽ���ʱ�̻���ͣ��������FULLģʽ����ôƵ���� NORMALģʽ���к�С�ļ���(�����ǲ�����)������Դ���ϵ������ݿ��𻵵��������ʵ���ϣ�����������ºܿ������Ӳ���Ѿ�����ʹ�ã����߷����������Ĳ��ɻָ���Ӳ������ 
    ����Ϊsynchronous OFF (0)ʱ��SQLite�ڴ������ݸ�ϵͳ�Ժ�ֱ�Ӽ���������ͣ��������SQLite��Ӧ�ó�������� ���ݲ������ˣ�����ϵͳ������д������ʱ����ϵ����������ݿ���ܻ��𻵡���һ���棬��synchronous OFFʱ һЩ�������ܻ��50���������ࡣ
    ��SQLite 2�У�ȱʡֵΪNORMAL.����3���޸�ΪFULL.
    
7 temp_store
    ʹ��2���ڴ�ģʽ��
    PRAGMA temp_store; 
    PRAGMA temp_store = DEFAULT; (0) 
    PRAGMA temp_store = FILE; (1) 
    PRAGMA temp_store = MEMORY; (2)
    ��ѯ�����"temp_store"���������á�
    ��temp_store����ΪDEFAULT (0),ʹ�ñ���ʱ��CԤ����� TEMP_STORE�����崢����ʱ�����ʱ������λ�á�
    ������ΪMEMORY (2)��ʱ�������������ڴ��С� 
    ������ΪFILE (1)�������ļ��С�temp_store_directorypragma ������ָ����Ÿ��ļ���Ŀ¼�����ı�temp_store���ã������Ѵ��ڵ���ʱ������������������ͼ��������ɾ����
    �����ԣ�����BBSӦ���ϣ�ͨ�����ϵ�����Ч�ʿ������2�����ϡ�










