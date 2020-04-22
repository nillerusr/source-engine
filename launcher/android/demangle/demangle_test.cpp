#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>

extern "C" {

extern char *__cxa_demangle (const char *mangled, char *buf, size_t *len,
                             int *status);

}

const char* names[] = {
    "_Z3fo5n",
    "_Z3fo5o",
    "_ZN1f1fE",
    "_Z1fv",
    "_Z1fi",
    "_Z3foo3bar",
    "_Zrm1XS_",
    "_ZplR1XS0_",
    "_ZlsRK1XS1_",
    "_ZN3FooIA4_iE3barE",
    "_Z1fIiEvi",
    "_Z5firstI3DuoEvS0_",
    "_Z5firstI3DuoEvT_",
    "_Z3fooIiFvdEiEvv",
    "_Z1fIFvvEEvv",
    "_ZN1N1fE",
    "_ZN6System5Sound4beepEv",
    "_ZN6SkPath4IterC1ERKS_b",
    "_ZN6SkPath4Iter4nextEP7SkPoint",
    "_ZN6SkScan8HairLineERK7SkPointS2_PK8SkRegionP9SkBlitter",
    "_Z3fooiPiPS_PS0_PS1_PS2_PS3_PS4_PS5_PS6_PS7_PS8_PS9_PSA_PSB_PSC_",
    "_Z1fILi1ELc120EEv1AIXplT_cviLd810000000000000000703DAD7A370C5EEE",
    "_ZZN7myspaceL3foo_1EvEN11localstruct1fEZNS_3fooEvE16otherlocalstruct",
    "_Z7ZipWithI7QStringS0_5QListZN4oral6detail16AdaptCreateTableI7AccountEES0_RKNS3_16CachedFieldsDataEEUlRKS0_SA_E_ET1_IDTclfp1_cvT__EcvT0__EEEERKT1_ISC_ERKT1_ISD_ET2_",
    NULL};

const char* expected[] = {
    "fo5(__int128)",
    "fo5(unsigned __int128)",
    "f::f",
    "f()",
    "f(int)",
    "foo(bar)",
    "operator%(X, X)",
    "operator+(X&, X&)",
    "operator<<(X const&, X const&)",
    "Foo<int [4]>::bar",
    "void f<int>(int)",
    "void first<Duo>(Duo)",
    "void first<Duo>(Duo)",
    "void foo<int, void (double), int>()",
    "void f<void ()>()",
    "N::f",
    "System::Sound::beep()",
    "SkPath::Iter::Iter(SkPath const&, bool)",
    "SkPath::Iter::next(SkPoint*)",
    "SkScan::HairLine(SkPoint const&, SkPoint const&, SkRegion const*, SkBlitter*)",
    "foo(int, int*, int**, int***, int****, int*****, int******, int*******, int********, int*********, int**********, int***********, int************, int*************, int**************, int***************)",
    "void f<1, (char)120>(A<(1)+((int)((double)[810000000000000000703DAD7A370C5]))>)",
    "myspace::foo()::localstruct::f(myspace::foo()::otherlocalstruct)",
    "QList<decltype ({parm#3}((QString)(), (QString)()))> ZipWith<QString, QString, QList, QString oral::detail::AdaptCreateTable<Account>(oral::detail::CachedFieldsData const&)::{lambda(QString const&, QString const&)#1}>(QList<QString oral::detail::AdaptCreateTable<Account>(oral::detail::CachedFieldsData const&)::{lambda(QString const&, QString const&)#1}> const&, QList<QList> const&, QString oral::detail::AdaptCreateTable<Account>(oral::detail::CachedFieldsData const&)::{lambda(QString const&, QString const&)#1})",
    ""
};

TEST(gcc_demangle, smoke) {
  for (int i = 0; names[i] != NULL; ++i) {
    char *demangled = __cxa_demangle(names[i], 0, 0, 0);
    ASSERT_STREQ(expected[i], demangled);
    free(demangled);
  }
}
