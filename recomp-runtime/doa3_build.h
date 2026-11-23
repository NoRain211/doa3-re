#ifndef RECOMP_DOA3_BUILD_H
#define RECOMP_DOA3_BUILD_H

/* A guest address that differs between DOA3 USA (1.0), Japan 3.1 and Europe
   3.2: the value for the lifted XBE. Japan values come from the Version
   Tracking map, see docs/doa3-jp-test.md; Europe values from the Japan map,
   see docs/doa3-eur-test.md. Europe is XDK 4134 like Japan, so a Europe build
   also defines RECOMP_DOA3_JP_BINDINGS, and DOA3_4134(japan, europe) picks the
   address in code only those two builds compile. */
#if defined(RECOMP_DOA3_EUR_BINDINGS)
#define DOA3(usa, japan, europe) (europe)
#define DOA3_4134(japan, europe) (europe)
#elif defined(RECOMP_DOA3_JP_BINDINGS)
#define DOA3(usa, japan, europe) (japan)
#define DOA3_4134(japan, europe) (japan)
#else
#define DOA3(usa, japan, europe) (usa)
#endif

#endif
