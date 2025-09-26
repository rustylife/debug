// SPDX-License-Identifier: GPL-2.0
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <linux/limits.h>
#include <oci.h>

static char libpath[PATH_MAX];
static char err_buf[OCI_ERROR_MAXMSG_SIZE2];
static void *handle;

static void print_error_and_exit(char *msg, sword err)
{
	printf("%s. OCI error code: %d\n", msg, err);
	exit(-1);
}

static void get_ora_error(OCIError *errhp, sword err)
{
	if (err == OCI_SUCCESS_WITH_INFO || err == OCI_NO_DATA || err != OCI_ERROR)
		return;

	sword (*OCIErrorGet)() = dlsym(handle, "OCIErrorGet");
	if (!OCIErrorGet) {
		printf("%s\n", dlerror());
		return;
	}
	sword err_out = OCI_SUCCESS;
	OCIErrorGet(errhp, 1, NULL, &err_out, err_buf, sizeof(err_buf), OCI_HTYPE_ERROR);
	printf("%s", err_buf);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: ./oracleocitest \"/opt/oracle/product/19c/dbhome1\" \"testdb\"\n");
		return -1;
	}

	char *orahome = argv[1];
	char *orasid = argv[2];
	OCIEnv *penv = NULL;
	OCIError *errhp = NULL;
	OCISvcCtx *psvcctx = NULL;
	OCIServer *psrv = NULL;
	OCISession *psession = NULL;
	sword ret;

	printf("ORACLE_HOME=%s\n", orahome);
	printf("ORACLE_SID=%s\n", orasid);
	setenv("ORACLE_HOME", orahome, TRUE);
	setenv("ORACLE_SID", orasid, TRUE);

	strcat(libpath, orahome);
	strcat(libpath, "/lib/libclntsh.so");
	printf("lib path: %s\n", libpath);
	handle = dlopen(libpath, RTLD_LAZY);
	if (!handle)
		goto dlfail;
	sword (*OCIEnvCreate)() = dlsym(handle, "OCIEnvCreate");
	if (!OCIEnvCreate)
		goto dlfail;
	sword (*OCIHandleAlloc)() = dlsym(handle, "OCIHandleAlloc");
	if (!OCIHandleAlloc)
		goto dlfail;
	sword (*OCIServerAttach)() = dlsym(handle, "OCIServerAttach");
	if (!OCIServerAttach)
		goto dlfail;
	sword (*OCIAttrSet)() = dlsym(handle, "OCIAttrSet");
	if (!OCIAttrSet)
		goto dlfail;
	sword (*OCISessionBegin)() = dlsym(handle, "OCISessionBegin");
	if (!OCISessionBegin)
		goto dlfail;

	ret = OCIEnvCreate((OCIEnv **) &penv, (ub4) OCI_THREADED, (dvoid *) 0, (dvoid * (*)(dvoid *, size_t)) 0,
			(dvoid * (*)(dvoid *, dvoid *, size_t)) 0, (void (*)(dvoid *, dvoid *)) 0, (size_t) 0, (dvoid **) 0);
	if (ret != OCI_SUCCESS)
		print_error_and_exit("Failed to create OCI env", ret);

	ret = OCIHandleAlloc((dvoid *) penv, (dvoid **) &errhp, OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) 0);
	if (ret != OCI_SUCCESS)
		print_error_and_exit("Failed to allocate OCIError handle", ret);

	ret = OCIHandleAlloc((dvoid *) penv, (dvoid **) &psvcctx, OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) 0);
	if (ret != OCI_SUCCESS)
		print_error_and_exit("Failed to allocate OCISvcCtx handle", ret);

	ret = OCIHandleAlloc((dvoid *) penv, (dvoid **) &psrv, OCI_HTYPE_SERVER, (size_t) 0, (dvoid **) 0);
	if (ret != OCI_SUCCESS)
		print_error_and_exit("Failed to allocate OCIServer handle", ret);

	ret = OCIServerAttach(psrv, errhp, 0, 0, OCI_DEFAULT);
	if (ret != OCI_SUCCESS) {
		get_ora_error(errhp, ret);
		print_error_and_exit("Failed to attach to server", ret);
	}

	ret = OCIAttrSet(psvcctx, OCI_HTYPE_SVCCTX, psrv, (ub4) 0, OCI_ATTR_SERVER, errhp);
	if (ret != OCI_SUCCESS) {
		get_ora_error(errhp, ret);
		print_error_and_exit("Failed to set attributes", ret);
	}

	ret = OCIHandleAlloc((dvoid *) penv, (dvoid **) &psession, OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0);
	if (ret != OCI_SUCCESS)
		print_error_and_exit("Failed to allocate OCISession handle", ret);

	ret = OCISessionBegin(psvcctx, errhp, psession, OCI_CRED_EXT, OCI_SYSDBA);
	if (ret != OCI_SUCCESS) {
		get_ora_error(errhp, ret);
		print_error_and_exit("Failed to start session", ret);
	}

	ret = OCIAttrSet(psvcctx, OCI_HTYPE_SVCCTX, psession, (ub4) 0, OCI_ATTR_SESSION, errhp);
	if (ret != OCI_SUCCESS) {
		get_ora_error(errhp, ret);
		print_error_and_exit("Failed to set attributes", ret);
	}

	printf("OCI test completed successfully\n");
	return 0;

dlfail:
	printf("%s\n", dlerror());
	return -1;
}
