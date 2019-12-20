// compile with: ole32.lib oleaut32.lib  
#define DBINITCONSTANTS  
#define OLEDBVER 0x0250   // to include correct interfaces  

#include <oledb.h>  
#include <oledberr.h>  
#include <stdio.h>  
#include <stddef.h>   // for offsetof  
#include <msoledbsql.h>  

// @type UWORD    | 2 byte unsigned integer.  
typedef unsigned short UWORD;

// @type SDWORD   | 4 byte signed integer.  
typedef signed long SDWORD;

WCHAR g_wszTable[] = L"IRFLTable";
WCHAR g_strTestLOC[100] = L"Localhost";
WCHAR g_strTestDBName[] = L"Test";
WCHAR integratedSecurity[] = L"SSPI";
const UWORD g_cOPTION = 4;
const UWORD MAXPROPERTIES = 5;
const ULONG DEFAULT_CBMAXLENGTH = 20;
IMalloc* g_pIMalloc = NULL;
IDBInitialize* g_pIDBInitialize = NULL;

// Given an ICommand pointer, properties, and query, a rowsetpointer is returned.  
HRESULT CreateSessionCommand(DBPROPSET* rgPropertySets, ULONG ulcPropCount, CLSID clsidProv);

// Use to set properties and execute a given query.  
HRESULT ExecuteQuery(IDBCreateCommand* pIDBCreateCommand,
    WCHAR* pwszQuery,
    DBPROPSET* rgPropertySets,
    ULONG ulcPropCount,
    LONG* pcRowsAffected,
    IRowset** ppIRowset,
    BOOL fSuccessOnly = TRUE);

// Use to set up options for call to IDBInitialize::Initialize.  
void SetupOption(DBPROPID PropID, WCHAR* wszVal, DBPROP* pDBProp);

// Sets fastload property on/off for session.  
HRESULT SetFastLoadProperty(BOOL fSet);

// IRowsetFastLoad inserting data.  
HRESULT FastLoadData();

// How to lay out each column in memory.  
struct COLUMNDATA {
    DBLENGTH dwLength;   // Length of data (not space allocated).  
    DWORD dwStatus;   // Status of column.  
    BYTE bData[1];   // Store data here as a variant.  
};

#define COLUMN_ALIGNVAL 8  

#define ROUND_UP(Size, Amount)(((DWORD)(Size) + ((Amount)-1)) & ~((Amount)-1))  

int main() {
    HRESULT hr = NOERROR;
    HRESULT hr2 = NOERROR;

    // OLE initialized?  
    BOOL fInitialized = FALSE;

    // One property set for initializing.  
    DBPROPSET rgPropertySets[1];

    // Properties within above property set.  
    DBPROP rgDBProperties[g_cOPTION];

    IDBCreateCommand* pIDBCreateCommand = NULL;
    IRowset* pIRowset = NULL;
    DBPROPSET* rgProperties = NULL;
    IAccessor* pIAccessor = NULL;

    // Basic initialization.  
    if (FAILED(CoInitialize(NULL)))
        goto cleanup;
    else
        fInitialized = TRUE;

    hr = CoGetMalloc(MEMCTX_TASK, &g_pIMalloc);
    if ((!g_pIMalloc) || FAILED(hr))
        goto cleanup;

    // Set up property set for call to IDBInitialize in CreateSessionCommand.  
    rgPropertySets[0].rgProperties = rgDBProperties;
    rgPropertySets[0].cProperties = g_cOPTION;
    rgPropertySets[0].guidPropertySet = DBPROPSET_DBINIT;

    SetupOption(DBPROP_INIT_CATALOG, g_strTestDBName, &rgDBProperties[0]);

    SetupOption(DBPROP_AUTH_INTEGRATED, integratedSecurity, &rgDBProperties[1]);

    SetupOption(DBPROP_INIT_DATASOURCE, g_strTestLOC, &rgDBProperties[3]);

    if (!SUCCEEDED(hr = CreateSessionCommand(rgPropertySets, 1, CLSID_MSOLEDBSQL)))
        goto cleanup;

    // Get IRowsetFastLoad and insert data into IRFLTable.  
    if (FAILED(hr = FastLoadData()))
        goto cleanup;

cleanup:
    // Release memory.  
    if (rgProperties && rgProperties->rgProperties)
        delete[](rgProperties->rgProperties);
    if (rgProperties)
        delete[]rgProperties;
    if (pIDBCreateCommand)
        pIDBCreateCommand->Release();

    if (pIAccessor)
        pIAccessor->Release();

    if (pIRowset)
        pIRowset->Release();
    if (g_pIMalloc)
        g_pIMalloc->Release();

    if (g_pIDBInitialize) {
        hr2 = g_pIDBInitialize->Uninitialize();
        if (FAILED(hr2))
            printf("Uninitialize failed\n");
    }

    if (fInitialized)
        CoUninitialize();

    if (SUCCEEDED(hr))
        printf("Test completed successfully.\n\n");
    else
        printf("Test failed.\n\n");
}

HRESULT FastLoadData() {
    HRESULT hr = E_FAIL;
    HRESULT hr2 = E_FAIL;
    DBID TableID;

    IDBCreateSession* pIDBCreateSession = NULL;
    IOpenRowset* pIOpenRowsetFL = NULL;
    IRowsetFastLoad* pIFastLoad = NULL;

    IAccessor* pIAccessor = NULL;
    HACCESSOR hAccessor = 0;
    DBBINDSTATUS oneStatus = 0;

    DBBINDING oneBinding;
    ULONG ulOffset = 0;
    TableID.uName.pwszName = NULL;
    LONG i = 0;
    void* pData = NULL;
    COLUMNDATA* pcolData = NULL;

    TableID.eKind = DBKIND_NAME;
    // if ( !(TableID.uName.pwszName = new WCHAR[wcslen(g_wszTable) + 2]) )  
    LPOLESTR x = TableID.uName.pwszName = new WCHAR[wcslen(g_wszTable) + 2];
    if (!x)
        return E_FAIL;
    wcsncpy_s(TableID.uName.pwszName, wcslen(g_wszTable) + 2, g_wszTable, wcslen(g_wszTable) + 1);
    TableID.uName.pwszName[wcslen(g_wszTable) + 1] = (WCHAR)NULL;

    // Get the fastload pointer.  
    if (FAILED(hr = SetFastLoadProperty(TRUE)))
        goto cleanup;

    if (FAILED(hr =
        g_pIDBInitialize->QueryInterface(IID_IDBCreateSession, (void**)&pIDBCreateSession)))
        goto cleanup;

    if (FAILED(hr =
        pIDBCreateSession->CreateSession(NULL, IID_IOpenRowset, (IUnknown**)&pIOpenRowsetFL)))
        goto cleanup;

    // Get IRowsetFastLoad initialized to use the test table.  
    if (FAILED(hr =
        pIOpenRowsetFL->OpenRowset(NULL,
            &TableID,
            NULL,
            IID_IRowsetFastLoad,
            0,
            NULL,
            (LPUNKNOWN*)&pIFastLoad)))
        goto cleanup;

    // Set up custom bindings.  
    oneBinding.dwPart = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
    oneBinding.iOrdinal = 1;
    oneBinding.pTypeInfo = NULL;
    oneBinding.obValue = ulOffset + offsetof(COLUMNDATA, bData);
    oneBinding.obLength = ulOffset + offsetof(COLUMNDATA, dwLength);
    oneBinding.obStatus = ulOffset + offsetof(COLUMNDATA, dwStatus);
    oneBinding.cbMaxLen = 1;   // Size of varchar column.  
    oneBinding.pTypeInfo = NULL;
    oneBinding.pObject = NULL;
    oneBinding.pBindExt = NULL;
    oneBinding.dwFlags = 0;
    oneBinding.eParamIO = DBPARAMIO_NOTPARAM;
    oneBinding.dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    oneBinding.bPrecision = 0;
    oneBinding.bScale = 0;
    oneBinding.wType = DBTYPE_BOOL;
    ulOffset = oneBinding.cbMaxLen + offsetof(COLUMNDATA, bData);
    ulOffset = ROUND_UP(ulOffset, COLUMN_ALIGNVAL);

    if (FAILED(hr =
        pIFastLoad->QueryInterface(IID_IAccessor, (void**)&pIAccessor)))
        return hr;

    if (FAILED(hr = pIAccessor->CreateAccessor(DBACCESSOR_ROWDATA,
        1,
        &oneBinding,
        ulOffset,
        &hAccessor,
        &oneStatus)))
        return hr;

    // Set up memory buffer.  
    pData = new BYTE[40];
    if (!(pData /* = new BYTE[40]*/)) {
        hr = E_FAIL;
        goto cleanup;
    }

    pcolData = (COLUMNDATA*)pData;
    pcolData->dwLength = 1;
    pcolData->dwStatus = 0;

    for (i = 0; i < 10; i++)
    {
        if (i % 2 == 0)
        {
            pcolData->bData[0] = 0x00;
        }
        else
        {
            pcolData->bData[0] = 0xFF;
        }
        

        if (FAILED(hr = pIFastLoad->InsertRow(hAccessor, pData)))
            goto cleanup;
    }

    if (FAILED(hr = pIFastLoad->Commit(TRUE)))
        printf("Error on IRFL::Commit\n");

cleanup:
    if (FAILED(hr2 = SetFastLoadProperty(FALSE)))
        printf("SetFastLoadProperty(FALSE) failed with %x", hr2);

    if (pIAccessor && hAccessor)
        if (FAILED(pIAccessor->ReleaseAccessor(hAccessor, NULL)))
            hr = E_FAIL;

    if (pIAccessor)
        pIAccessor->Release();

    if (pIFastLoad)
        pIFastLoad->Release();

    if (pIOpenRowsetFL)
        pIOpenRowsetFL->Release();

    if (pIDBCreateSession)
        pIDBCreateSession->Release();

    if (TableID.uName.pwszName)
        delete[]TableID.uName.pwszName;

    return hr;
}

HRESULT SetFastLoadProperty(BOOL fSet) {
    HRESULT hr = S_OK;
    IDBProperties* pIDBProps = NULL;
    DBPROP rgProps[1];
    DBPROPSET PropSet;

    VariantInit(&rgProps[0].vValue);

    rgProps[0].dwOptions = DBPROPOPTIONS_REQUIRED;
    rgProps[0].colid = DB_NULLID;
    rgProps[0].vValue.vt = VT_BOOL;
    rgProps[0].dwPropertyID = SSPROP_ENABLEFASTLOAD;

    if (fSet == TRUE)
        rgProps[0].vValue.boolVal = VARIANT_TRUE;
    else
        rgProps[0].vValue.boolVal = VARIANT_FALSE;

    PropSet.rgProperties = rgProps;
    PropSet.cProperties = 1;
    PropSet.guidPropertySet = DBPROPSET_SQLSERVERDATASOURCE;

    if (SUCCEEDED(hr = g_pIDBInitialize->QueryInterface(IID_IDBProperties, (LPVOID*)&pIDBProps)))
        hr = pIDBProps->SetProperties(1, &PropSet);

    VariantClear(&rgProps[0].vValue);

    if (pIDBProps)
        pIDBProps->Release();

    return hr;
}

HRESULT CreateSessionCommand(DBPROPSET* rgPropertySets,// @parm [in] property sets  
    ULONG ulcPropCount,   // @parm [in] count of prop sets.  
    CLSID clsidProv) {  // @parm [in] Provider CLSID.  

    HRESULT hr = NOERROR;
    IDBCreateSession* pIDBCreateSession = NULL;
    IDBProperties* pIDBProperties = NULL;
    UWORD i = 0, j = 0;   // indexes.  

    if (ulcPropCount && !rgPropertySets) {
        hr = E_INVALIDARG;
        return hr;
    }

    if (!SUCCEEDED(hr = CoCreateInstance(clsidProv,
        NULL, CLSCTX_INPROC_SERVER,
        IID_IDBInitialize,
        (void**)&g_pIDBInitialize)))
        goto CLEANUP;

    if (!SUCCEEDED(hr = g_pIDBInitialize->QueryInterface(IID_IDBProperties,
        (void**)&pIDBProperties)))
        goto CLEANUP;

    if (!SUCCEEDED(hr = pIDBProperties->SetProperties(ulcPropCount, rgPropertySets)))
        goto CLEANUP;

    if (!SUCCEEDED(hr = g_pIDBInitialize->Initialize())) {
        printf("Call to initialize failed.\n");
        goto CLEANUP;
    }

CLEANUP:
    if (pIDBProperties)
        pIDBProperties->Release();
    if (pIDBCreateSession)
        pIDBCreateSession->Release();

    for (i = 0; i < ulcPropCount; i++)
        for (j = 0; j < rgPropertySets[i].cProperties; j++)
            VariantClear(&(rgPropertySets[i].rgProperties[j]).vValue);
    return hr;
}

void SetupOption(DBPROPID PropID, WCHAR* wszVal, DBPROP* pDBProp) {
    pDBProp->dwPropertyID = PropID;
    pDBProp->dwOptions = DBPROPOPTIONS_REQUIRED;
    pDBProp->colid = DB_NULLID;
    pDBProp->vValue.vt = VT_BSTR;
    pDBProp->vValue.bstrVal = SysAllocStringLen(wszVal, wcslen(wszVal));
}