// RUN: 3c -base-dir=%S -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
#define VSFTP_COMMAND_FD 1

struct vsf_sysutil_statbuf {
  int x;
};

void vsf_sysutil_fstat(int fd, struct vsf_sysutil_statbuf **ptr) {
  //CHECK: void vsf_sysutil_fstat(int fd, struct vsf_sysutil_statbuf **ptr : itype(_Ptr<_Ptr<struct vsf_sysutil_statbuf>>)) {
  *ptr = (struct vsf_sysutil_statbuf *)3;
}

int vsf_sysutil_statbuf_is_socket(struct vsf_sysutil_statbuf *ptr) {
  //CHECK: int vsf_sysutil_statbuf_is_socket(_Ptr<struct vsf_sysutil_statbuf> ptr) _Checked {
  return 1;
}

void vsf_sysutil_free(struct vsf_sysutil_statbuf *ptr) {
  //CHECK: void vsf_sysutil_free(_Ptr<struct vsf_sysutil_statbuf> ptr) _Checked {
}

void die(const char *s) {
  //CHECK: void die(const char *s : itype(_Ptr<const char>)) {
  s = (char *)3;
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

static void do_sanity_checks(void) {
  //CHECK: static void do_sanity_checks(void) _Checked {
  {
    //CHECK: {
    struct vsf_sysutil_statbuf *p_statbuf = 0;
    //CHECK: _Ptr<struct vsf_sysutil_statbuf> p_statbuf = 0;
    vsf_sysutil_fstat(VSFTP_COMMAND_FD, &p_statbuf);
    if (!vsf_sysutil_statbuf_is_socket(p_statbuf)) {
      //CHECK: if (!vsf_sysutil_statbuf_is_socket(p_statbuf)) {
      die("vsftpd: not configured for standalone, must be started from inetd");
    }
    vsf_sysutil_free(p_statbuf);
  }
  if (tunable_one_process_model) {
    if (tunable_local_enable) {
      //CHECK: if (tunable_local_enable) {
      die("vsftpd: security: 'one_process_model' is anonymous only");
    }
    if (!vsf_sysdep_has_capabilities_as_non_root()) {
      //CHECK: if (!vsf_sysdep_has_capabilities_as_non_root()) {
      die("vsftpd: security: 'one_process_model' needs a better OS");
    }
  }
  if (!tunable_local_enable && !tunable_anonymous_enable) {
    //CHECK: if (!tunable_local_enable && !tunable_anonymous_enable) {
    die("vsftpd: both local and anonymous access disabled!");
  }
  if (!tunable_ftp_enable && !tunable_http_enable) {
    //CHECK: if (!tunable_ftp_enable && !tunable_http_enable) {
    die("vsftpd: both FTP and HTTP disabled!");
  }
  if (tunable_http_enable && !tunable_one_process_model) {
    //CHECK: if (tunable_http_enable && !tunable_one_process_model) {
    die("vsftpd: HTTP needs 'one_process_model' for now");
  }
}
