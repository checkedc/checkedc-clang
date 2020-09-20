// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
#define VSFTP_COMMAND_FD 1

struct vsf_sysutil_statbuf { 
  int x;
};

void vsf_sysutil_fstat(int fd, struct vsf_sysutil_statbuf** ptr) { 
  //CHECK: void vsf_sysutil_fstat(int fd, _Ptr<struct vsf_sysutil_statbuf *> ptr) {
  *ptr = (struct vsf_sysutil_statbuf*) 3;
}

int vsf_sysutil_statbuf_is_socket(struct vsf_sysutil_statbuf* ptr) { 
  //CHECK: int vsf_sysutil_statbuf_is_socket(struct vsf_sysutil_statbuf *ptr : itype(_Ptr<struct vsf_sysutil_statbuf>)) _Checked {
  return 1;
}

void vsf_sysutil_free(struct vsf_sysutil_statbuf* ptr) { 
//CHECK: void vsf_sysutil_free(struct vsf_sysutil_statbuf *ptr : itype(_Ptr<struct vsf_sysutil_statbuf>)) _Checked {
}

void die(const char *s) {
//CHECK: void die(const char *s) {
  s = (char*) 3;
}

int tunable_one_process_model = 1;
int tunable_local_enable = 1;
int tunable_anonymous_enable = 1;
int tunable_ftp_enable = 1;
int tunable_http_enable = 1;

int vsf_sysdep_has_capabilities_as_non_root(void) { 
  //CHECK: int vsf_sysdep_has_capabilities_as_non_root(void) _Checked { 
  return 1;
}

static void
do_sanity_checks(void)
{
//CHECK: _Checked {
  {
    //CHECK: _Unchecked {
    struct vsf_sysutil_statbuf* p_statbuf = 0;
    //CHECK: struct vsf_sysutil_statbuf* p_statbuf = 0;
    vsf_sysutil_fstat(VSFTP_COMMAND_FD, &p_statbuf);
    if (!vsf_sysutil_statbuf_is_socket(p_statbuf))
    {
    //CHECK: {
      die("vsftpd: not configured for standalone, must be started from inetd");
    }
    vsf_sysutil_free(p_statbuf);
  }
  if (tunable_one_process_model)
  {
    if (tunable_local_enable)
    {
    //CHECK: _Unchecked {
      die("vsftpd: security: 'one_process_model' is anonymous only");
    }
    if (!vsf_sysdep_has_capabilities_as_non_root())
    {
    //CHECK: _Unchecked {
      die("vsftpd: security: 'one_process_model' needs a better OS");
    }
  }
  if (!tunable_local_enable && !tunable_anonymous_enable)
  {
    //CHECK: _Unchecked {
    die("vsftpd: both local and anonymous access disabled!");
  }
  if (!tunable_ftp_enable && !tunable_http_enable)
  {
    //CHECK: _Unchecked {
    die("vsftpd: both FTP and HTTP disabled!");
  }
  if (tunable_http_enable && !tunable_one_process_model)
  {
    //CHECK: _Unchecked {
    die("vsftpd: HTTP needs 'one_process_model' for now");
  }
}
