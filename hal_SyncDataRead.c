
#include "hal_SyncDataRead.h"
#define _WINSOCKAPI_
#include <windows.h>
#include "pthread.h"
#include "crt.h"
#include "dc.h"

#ifdef SDR_SELFTEST
#include "Logger.h"
pthread_mutex_t mutexLogger = PTHREAD_MUTEX_INITIALIZER;
#endif /* SDR_SELFTEST */

#define DEF_BLOCK_COUNT_IN_POOL 100
#define DEF_MAX_REALLOC_COUNT 5

typedef struct  
{
    PUINT8 pData;
    UINT32 DataSize;
}S_DATA_BLOCK;

typedef struct
{
    PUINT8 pData;
    UINT32 DataSize;
    UINT32 BuffSize;
}S_DATA_BLOCK_POOL;

typedef struct 
{
    pthread_t pthreadId;
    pthread_mutex_t mutexRead;
    pthread_mutex_t mutexWrite;

    BOOL IsRunned;

    /** Максимальный размер буфера для чтения и записи. */
    UINT32 ResiveBuffSize;
    /** Буфер для чтения. */
    S_DATA_BLOCK ReadedData;
    /** Буфер для записи. */
    S_DATA_BLOCK WritedData;
    /** Пул накопленных данных. Однонаправленный список. */
    S_DATA_BLOCK_POOL DataPool;

    /* На будущее
    PS_S_HAL_SDR_ADDDATA pAddData; */
}S_SDR_CTX, *PR_S_SDR_CTX;

static S_SDR_CTX m_Sdr_Ctx;

/** Мютикс проверки использования функционала. */
static pthread_mutex_t m_mutexIsRun;

static void * aux_ReadData(void *args)
{
    URC result = URC_OK;
    PR_S_SDR_CTX arg = (PR_S_SDR_CTX)args;

    PUINT8 pBuff = arg->DataPool.pData;
    UINT32 BuffSize = 0;
    UINT32 SizeFirstData = 0;
    BOOL isDataReaded = FALSE;
    UINT8 tmpBuff[4];
    UINT8 AvalebleReallocCount = DEF_MAX_REALLOC_COUNT;
    BOOL isNoMem = FALSE;

    pthread_detach(pthread_self());

    FOREVER
    {
        isDataReaded = FALSE;
        BuffSize = 0;
        if (isNoMem == FALSE)
        {
            pthread_mutex_lock(&(arg->mutexWrite));
            if (arg->WritedData.DataSize != 0)
            {
                MemCpy(pBuff, arg->WritedData.pData, arg->WritedData.DataSize);
                BuffSize = arg->WritedData.DataSize;
                arg->WritedData.DataSize = 0;
            }
            pthread_mutex_unlock(&(arg->mutexWrite));
        }
        else
        {
            isNoMem = FALSE;
        }

        /** Первое чтение данных. */
        if (SizeFirstData == 0 && BuffSize != 0)
            SizeFirstData = BuffSize;

//        Logger_DumpBuffer("pReadData", pBuff, (UINT16)BuffSize);
//        Logger_DumpBuffer("pool begin", arg->DataPool.pData, (UINT16)arg->DataPool.DataSize);

        result = URC_OK;
        if (pthread_mutex_trylock(&m_mutexIsRun) != EBUSY)
        {
            /** Если завершение работы. Проверяем тут для минимизации вызовов блокировки потоков. */
            if (arg->IsRunned == FALSE)
                result = URC_HAL_SYNCDATAREADE_CLOSED;
            pthread_mutex_unlock(&m_mutexIsRun);
        }

        if (URC_FAILED(result))
            break;

        if (pthread_mutex_trylock(&(arg->mutexRead)) != EBUSY)
        {
            /* pthread_mutex_lock не вызываем поток и так в lock */

            if (arg->ReadedData.DataSize == 0 && SizeFirstData != 0)
            {
                // copy from pool to buffer
                MemCpy(arg->ReadedData.pData, arg->DataPool.pData, SizeFirstData);
                arg->ReadedData.DataSize = SizeFirstData;
                isDataReaded = TRUE;
            }
            pthread_mutex_unlock(&(arg->mutexRead));
        }

        // SizeFirstData - First data len
        // First data | next data len | Next data | next data len | Next data
        if (isDataReaded == TRUE && arg->DataPool.DataSize > 0) // more then 1 block
        {
            // SizeFirstData - First data len = 0
            // First data | next data len | Next data | next data len | Next data
            //              ^ 0000000A                  ^ 00000000      ^
            //                                                          pBuff
            //                                                          ^
            //                                                          DataPool.DataSize
            // ----------->>>>>>
            // SizeFirstData - First data len = 0
            // First data | next data len | Next data | next data len | Next data 
            //              ^ 0000000A                  ^ 0000000B      ^
            //                                                          pBuff
            //                                                          ^
            //                                                          DataPool.DataSize
            // ----------->>>>>>
            // SizeFirstData - First data len = 0000000A
            // First data | next data len | Next data
            //              ^ 0000000B      ^
            //                              pBuff
            //                              ^
            //                              DataPool.DataSize

            if (BuffSize > 0)
            {
                /** Convert len to buffer */
                tmpBuff[0] = (UINT8)(BuffSize >> 24);
                tmpBuff[1] = (UINT8)(BuffSize >> 16);
                tmpBuff[2] = (UINT8)(BuffSize >> 8);
                tmpBuff[3] = (UINT8)BuffSize;

                /** Save len in pool */
                MemCpy(pBuff - sizeof(UINT32), tmpBuff, 4);
                MemSet(pBuff + BuffSize, 0, sizeof(UINT32));
                pBuff += BuffSize + sizeof(UINT32);
                arg->DataPool.DataSize += BuffSize + sizeof(UINT32);
            }

            /** Read len of next data */
            dc_HexToUINT32(arg->DataPool.pData + SizeFirstData, 4, &BuffSize);

            /** Remove first data */
            MemMove(arg->DataPool.pData, arg->DataPool.pData + SizeFirstData + sizeof(UINT32), (arg->DataPool.DataSize - ((arg->DataPool.pData + SizeFirstData) - arg->DataPool.pData)) + sizeof(UINT32));
            pBuff = pBuff - (SizeFirstData + sizeof(UINT32));
            arg->DataPool.DataSize = arg->DataPool.DataSize - (SizeFirstData + sizeof(UINT32));
            SizeFirstData = BuffSize;
        }
        if (isDataReaded == TRUE && arg->DataPool.DataSize == 0) // only 1 block
        {
            // SizeFirstData - First data len
            // First data |
            // ^   
            // pBuff
            // ^
            // DataPool.DataSize
            SizeFirstData = 0;
        }
        if (isDataReaded == FALSE && arg->DataPool.DataSize > 0 && BuffSize > 0) // more then 1 block
        {
            // SizeFirstData - First data len
            // First data | next data len | Next data | next data len | Next data
            //              ^ 0000000A                  ^ 00000000      ^
            //                                                          pBuff
            //                                                          ^
            //                                                          DataPool.DataSize

            /** Convert len to buffer */
            tmpBuff[0] = (UINT8)(BuffSize >> 24);
            tmpBuff[1] = (UINT8)(BuffSize >> 16);
            tmpBuff[2] = (UINT8)(BuffSize >> 8);
            tmpBuff[3] = (UINT8)BuffSize;

            /** Save len in pool */
            MemCpy(pBuff - sizeof(UINT32), tmpBuff, 4);
            MemSet(pBuff + BuffSize, 0, sizeof(UINT32));
            pBuff += BuffSize + sizeof(UINT32);
            arg->DataPool.DataSize += BuffSize + sizeof(UINT32);
        }
        if (isDataReaded == FALSE && arg->DataPool.DataSize == 0 && BuffSize > 0) // first block
        {
            // SizeFirstData - First data len
            // First data | next data len | Next data
            //              ^ 00000000      ^
            //                              pBuff
            //                              ^
            //                              DataPool.DataSize
            MemSet(pBuff + BuffSize, 0, sizeof(UINT32));
            pBuff += BuffSize + sizeof(UINT32);
            arg->DataPool.DataSize = BuffSize + sizeof(UINT32);
        }
        if (arg->DataPool.DataSize > 0 && arg->DataPool.BuffSize - arg->DataPool.DataSize < arg->ResiveBuffSize + sizeof(UINT32))
        {
            if (AvalebleReallocCount > 0)
            {
#ifdef SDR_SELFTEST
/*
                pthread_mutex_lock(&mutexLogger);
                Logger_DumpBuffer("pool realloc begin data", arg->DataPool.pData, (UINT16)arg->DataPool.DataSize);
                Logger_DumpBuffer("pData 1", &arg->DataPool.pData, (UINT16)4);
                Logger_DumpBuffer("pBuff 1", &pBuff, (UINT16)4);
*/
#endif /* SDR_SELFTEST */
                BuffSize = pBuff - arg->DataPool.pData;
                arg->DataPool.BuffSize = (UINT32)(arg->DataPool.BuffSize + ((sizeof(UINT32) + arg->ResiveBuffSize) * DEF_BLOCK_COUNT_IN_POOL));
                arg->DataPool.pData = (PUINT8)realloc(arg->DataPool.pData, (size_t)arg->DataPool.BuffSize);
                if (arg->DataPool.pData == NULL)
                {
                    // Нехватка памяти !!!!
                    break;
                }
                pBuff = arg->DataPool.pData + BuffSize;
#ifdef SDR_SELFTEST
/*
                Logger_DumpBuffer("pData 2", &arg->DataPool.pData, (UINT16)4);
                Logger_DumpBuffer("pBuff 2", &pBuff, (UINT16)4);
                Logger_DumpBuffer("pool realloc begin end", arg->DataPool.pData, (UINT16)arg->DataPool.DataSize);
                pthread_mutex_unlock(&mutexLogger);
*/
#endif /* SDR_SELFTEST */
                AvalebleReallocCount--;
            }
            else
            {
                isNoMem = TRUE;
            }
        }

        Sleep(50);
    }

    return NULL;
} /* aux_ReadData */

URC hal_SYNCDATAREAD_Initialize(void)
{
    //m_mutexIsRun = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    if (pthread_mutex_init(&m_mutexIsRun, NULL) != 0)
        return URC_HAL_SYNCDATAREADE_INITIALIZATION_ERROR_CREATE_MUTEX_1; // Error create mutex

    MemSet(&m_Sdr_Ctx, 0, sizeof(S_SDR_CTX));
    m_Sdr_Ctx.IsRunned = FALSE;

    return URC_OK;
} /* hal_SYNCDATAREAD_Initialize */

URC hal_SYNCDATAREAD_Uninitialize(void)
{
    URC Res = URC_OK;

    Res = hal_SYNCDATAREAD_Close();
    pthread_mutex_destroy(&m_mutexIsRun);

    return Res;
} /* hal_SYNCDATAREAD_Uninitialize */

URC hal_SYNCDATAREAD_Open(UINT32 ResiveBuffSize)
{
    URC Res = URC_OK;
    TRY_INIT();

    TRY
    {
        /** Чтение разделяемого ресурса допускается. Запись ведётся только в данном потоке. */
        if (m_Sdr_Ctx.IsRunned == TRUE)
            THROW_EXCEPTION(URC_OK);

        /** */
        MemSet(&m_Sdr_Ctx, 0, sizeof(S_SDR_CTX));

        /** */
        m_Sdr_Ctx.ResiveBuffSize = ResiveBuffSize;

        /** Буфер для чтения данных. */
        m_Sdr_Ctx.ReadedData.pData = (PUINT8)malloc((size_t)ResiveBuffSize);
        if (m_Sdr_Ctx.ReadedData.pData == NULL)
            THROW_EXCEPTION(URC_HAL_SYNCDATAREADE_OUT_OF_MEMORY_1);

        /** Буфер для записи данных. */
        m_Sdr_Ctx.WritedData.pData = (PUINT8)malloc((size_t)ResiveBuffSize);
        if (m_Sdr_Ctx.WritedData.pData == NULL)
            THROW_EXCEPTION(URC_HAL_SYNCDATAREADE_OUT_OF_MEMORY_2);

        /** Буфер для накопления данных*/
        m_Sdr_Ctx.DataPool.BuffSize = (sizeof(UINT32) + ResiveBuffSize) * DEF_BLOCK_COUNT_IN_POOL;
        m_Sdr_Ctx.DataPool.pData = (PUINT8)malloc((size_t)m_Sdr_Ctx.DataPool.BuffSize);
        if (m_Sdr_Ctx.DataPool.pData == NULL)
            THROW_EXCEPTION(URC_HAL_SYNCDATAREADE_OUT_OF_MEMORY_3);

        //m_Sdr_Ctx.mutexRead = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
        //m_Sdr_Ctx.mutexWrite = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

        /** */
        if (pthread_mutex_init(&(m_Sdr_Ctx.mutexRead), NULL) != 0)
            THROW_EXCEPTION(URC_HAL_SYNCDATAREADE_INITIALIZATION_ERROR_CREATE_MUTEX_2); // Error create mutex
        if (pthread_mutex_init(&(m_Sdr_Ctx.mutexWrite), NULL) != 0)
        {
            pthread_mutex_destroy(&(m_Sdr_Ctx.mutexRead));
            THROW_EXCEPTION(URC_HAL_SYNCDATAREADE_INITIALIZATION_ERROR_CREATE_MUTEX_3); // Error create mutex
        }

        /** */
        m_Sdr_Ctx.IsRunned = TRUE;
        if (pthread_create(&m_Sdr_Ctx.pthreadId, NULL, aux_ReadData, &m_Sdr_Ctx) != 0)
        {
            pthread_mutex_lock(&m_mutexIsRun);
            m_Sdr_Ctx.IsRunned = FALSE;
            pthread_mutex_unlock(&m_mutexIsRun);

            pthread_mutex_destroy(&(m_Sdr_Ctx.mutexRead));
            pthread_mutex_destroy(&(m_Sdr_Ctx.mutexWrite));
            THROW_EXCEPTION(URC_HAL_SYNCDATAREADE_INITIALIZATION_ERROR_CREATE_THREAD); // Error create thread
        }

        
    }
    CATCH
    {
        Res = GET_EXCEPTION_CODE();
        switch (Res)
        {
        case URC_OK:
            break;
        default:
            if (m_Sdr_Ctx.ReadedData.pData != NULL)
                free(m_Sdr_Ctx.ReadedData.pData);
            if (m_Sdr_Ctx.WritedData.pData != NULL)
                free(m_Sdr_Ctx.WritedData.pData);
            if (m_Sdr_Ctx.DataPool.pData != NULL)
                free(m_Sdr_Ctx.DataPool.pData);
            URC_LOG(Res);
            break;
        }
    }

    return Res;    
} /* hal_SYNCDATAREAD_Open */

URC hal_SYNCDATAREAD_Close(void)
{
    if (m_Sdr_Ctx.IsRunned == TRUE)
    {
        pthread_mutex_lock(&m_mutexIsRun);
        m_Sdr_Ctx.IsRunned = FALSE;
        pthread_mutex_unlock(&m_mutexIsRun);
        pthread_join(m_Sdr_Ctx.pthreadId, NULL);
        pthread_mutex_destroy(&(m_Sdr_Ctx.mutexRead));
        pthread_mutex_destroy(&(m_Sdr_Ctx.mutexWrite));
        // remove buffers
        if (m_Sdr_Ctx.ReadedData.pData != NULL)
            free(m_Sdr_Ctx.ReadedData.pData);
        if (m_Sdr_Ctx.WritedData.pData != NULL)
            free(m_Sdr_Ctx.WritedData.pData);
        if (m_Sdr_Ctx.DataPool.pData != NULL)
            free(m_Sdr_Ctx.DataPool.pData);
        memset(&m_Sdr_Ctx, 0, sizeof(S_SDR_CTX));
    }

    return URC_OK;
} /* hal_SYNCDATAREAD_Close */

URC hal_SYNCDATAREAD_GetData(void)
{
    BOOL isData = FALSE;

    /** Чтение разделяемого ресурса допускается. Запись ведётся только в данном потоке. */
    if (m_Sdr_Ctx.IsRunned == FALSE)
        return URC_HAL_SYNCDATAREADE_CLOSED;

    if (pthread_mutex_trylock(&(m_Sdr_Ctx.mutexRead)) == EBUSY)
        return URC_HAL_SYNCDATAREADE_NODATA;

    /* pthread_mutex_lock не вызываем поток и так в lock */

    if (m_Sdr_Ctx.ReadedData.DataSize != 0)
        isData = TRUE;
    pthread_mutex_unlock(&(m_Sdr_Ctx.mutexRead));

    if (isData == FALSE)
        return URC_HAL_SYNCDATAREADE_NODATA;

    return URC_OK;
} /* hal_SYNCDATAREAD_GetData */

URC hal_SYNCDATAREAD_ReceiveData(PUINT8 pBlock, PUINT32 pnBlockSize/*, PS_S_HAL_SDR_ADDDATA p_pAddData*/)
{
    /** Чтение разделяемого ресурса допускается. Запись ведётся только в данном потоке. */
    if (m_Sdr_Ctx.IsRunned == FALSE)
        return URC_HAL_SYNCDATAREADE_CLOSED;

    pthread_mutex_lock(&(m_Sdr_Ctx.mutexRead));
    if (m_Sdr_Ctx.ReadedData.DataSize != 0)
    {
        MemCpy(pBlock, m_Sdr_Ctx.ReadedData.pData, m_Sdr_Ctx.ReadedData.DataSize);
        *pnBlockSize = m_Sdr_Ctx.ReadedData.DataSize;
        m_Sdr_Ctx.ReadedData.DataSize = 0;
    }
    pthread_mutex_unlock(&(m_Sdr_Ctx.mutexRead));

    return URC_OK;
} /* hal_SYNCDATAREAD_ReceiveData */

URC hal_SYNCDATAREAD_WriteData(PUINT8 pBlock, UINT32 nDataSize/*, PS_S_HAL_SDR_ADDDATA p_pAddData*/)
{
    URC nResult = URC_OK;

    /** Синхронизация с основным потоком. Т.к. он может поменять значение. */
    pthread_mutex_trylock(&m_mutexIsRun);
    if (m_Sdr_Ctx.IsRunned == FALSE)
        nResult = URC_HAL_SYNCDATAREADE_CLOSED;
    pthread_mutex_unlock(&m_mutexIsRun);

    if (URC_FAILED(nResult))
        return nResult;

    if (m_Sdr_Ctx.ResiveBuffSize < nDataSize)
        return URC_HAL_SYNCDATAREADE_MORE_DATA_THAN_ALLOWED;

    /** Запись данных в пул. */
    pthread_mutex_lock(&(m_Sdr_Ctx.mutexWrite));
    if (m_Sdr_Ctx.WritedData.DataSize == 0)
    {
        MemCpy(m_Sdr_Ctx.WritedData.pData, pBlock, nDataSize);
        m_Sdr_Ctx.WritedData.DataSize = nDataSize;
    }
    else
    {
        nResult = URC_HAL_SYNCDATAREADE_REPEATE_WRITE;
    }
    pthread_mutex_unlock(&(m_Sdr_Ctx.mutexWrite));

    return nResult;
} /* hal_SYNCDATAREAD_WriteData */


#ifdef SDR_SELFTEST

#include "hal_msc.h"
#include "crt_cmn.h"

#define DEF_MAK_BLOCK_WRITE_COUNT 100

typedef struct
{
    UINT8 ThreadId;
    UINT32 SizeDataSend;
    UINT16 Count;
}S_TEST_CTX, *PS_TEST_CTX;


/*static URC aux_FisicalReadData(PUINT8 pData, PUINT32 pnSize, PS_S_HAL_SDR_ADDDATA * pAddData)
{
    UINT8 i;
    UINT8 j;

    i = CRT_GetRandom(0, 9);
    j = CRT_GetRandom(1, (UINT8)*pnSize);

    MemSet(pData, i + 0x30, j);
    *pnSize = (UINT32)j;

    return URC_OK;
}*/

void * aux_WriteData(void *args)
{
    URC nRes = URC_OK;
    UINT8 tmpBuff[2048];
    UINT32 DataSize = 50;
    UINT8 i;
    UINT8 j;
 //   UINT16 k;
 //   CHAR str[20];

    //URC_HAL_SYNCDATAREADE_REPEATE_WRITE
    //URC_HAL_SYNCDATAREADE_CLOSED

    PS_TEST_CTX arg = (PS_TEST_CTX)args;
    CRT_SeedRandom(arg->ThreadId * 5);
    arg->Count = 0;

    do 
    {
        i = CRT_GetRandom(0, 9);
        j = CRT_GetRandom(1, (UINT8)DataSize);
        MemSet(tmpBuff, i + 0x30, j);

        if (arg->Count < DEF_MAK_BLOCK_WRITE_COUNT)
            arg->Count++;
        else
            break;

        do 
        {
/*          sprintf(str, "%s = %d:", "aux_WriteData", *arg);
            pthread_mutex_lock(&mutexLogger);
            Logger_DumpBuffer(str, tmpBuff, (UINT16)j);
            pthread_mutex_unlock(&mutexLogger);
*/

            nRes = hal_SYNCDATAREAD_WriteData(tmpBuff, j);
            if (nRes != URC_HAL_SYNCDATAREADE_REPEATE_WRITE)
                break;
            else
            {
/*
                pthread_mutex_lock(&mutexLogger);
                Logger_DumpString("Repeate");
                pthread_mutex_unlock(&mutexLogger);
*/
            }

        } while (1);

        if (nRes == URC_HAL_SYNCDATAREADE_CLOSED)
        {
/*
            pthread_mutex_lock(&mutexLogger);
            Logger_DumpString("Closed");
            pthread_mutex_unlock(&mutexLogger);
*/
            break;
        }

        arg->SizeDataSend += j;

    } while (1);

    return NULL;
}


URC hal_SYNCDATAREAD_SelfTest(void)
{
    UINT8 tmpBuff[2048];
    UINT32 DataSize = 50;

    pthread_t pthreadId1;
    pthread_t pthreadId2;
    pthread_t pthreadId3;

    S_TEST_CTX num1;
    S_TEST_CTX num2;
    S_TEST_CTX num3;

    UINT16 Count;
    UINT32 ReadSize;
    UINT8 i;

    Count = 0;
    ReadSize = 0;

    //hal_SYNCDATAREAD_Initialize();

/*    hal_SYNCDATAREAD_Open(50);
    Logger_DumpString("Begin");*/

    Logger_DumpString("Begin");

    pthread_mutex_init(&mutexLogger, NULL);

    hal_SYNCDATAREAD_Open(50);

    num1.ThreadId = 1;
    num1.SizeDataSend = 0;
    num1.Count = 0;
    pthread_create(&pthreadId1, NULL, aux_WriteData, &num1);
    num2.ThreadId = 2;
    num2.SizeDataSend = 0;
    num2.Count = 0;
    pthread_create(&pthreadId2, NULL, aux_WriteData, &num2);
    num3.ThreadId = 3;
    num3.SizeDataSend = 0;
    num3.Count = 0;
    pthread_create(&pthreadId3, NULL, aux_WriteData, &num3);

    //hal_SYNCDATAREAD_Open(50);
    
    do 
    {
        DataSize = 50;
        if (hal_SYNCDATAREAD_GetData() == URC_OK)
        {
            if (hal_SYNCDATAREAD_ReceiveData(tmpBuff, &DataSize) == URC_OK)
            {
/*
                pthread_mutex_lock(&mutexLogger);
                Logger_DumpBuffer("SyncDataRead_ReceiveData", tmpBuff, (UINT16)DataSize);
                pthread_mutex_unlock(&mutexLogger);
*/
                ReadSize += DataSize;
                Count++;
            }

            (void)hal_Delay(100); // Читающий поток выбирает данные с задержшкой
        }

        if (Count >= DEF_MAK_BLOCK_WRITE_COUNT * 3)
            break;

    } while (1);

    hal_SYNCDATAREAD_Close();

    pthread_join(pthreadId1, NULL);
    pthread_join(pthreadId2, NULL);
    pthread_join(pthreadId3, NULL);

    hal_SYNCDATAREAD_Close();

    if (ReadSize != num1.SizeDataSend + num2.SizeDataSend + num3.SizeDataSend)
    {
        i = 1;
    }
    else
    {
        i = 2;
    }

    pthread_mutex_destroy(&mutexLogger);

    //hal_SYNCDATAREAD_Uninitialize();

    return URC_OK;
} /* hal_SYNCDATAREAD_SelfTest */

#endif /* SDR_SELFTEST */
