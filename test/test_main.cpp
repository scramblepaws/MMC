#include <QCoreApplication>
#include <cstdio>

#include "core/Vault.h"
#include "test_harness.h"

void test_crypto(TestRunner& runner);
void test_vault(TestRunner& runner);
void test_mime(TestRunner& runner);
void test_importer(TestRunner& runner);
void test_imap(TestRunner& runner);

int main(int argc, char** argv)
{
    // Unbuffered, so the output survives a crash.
    setvbuf(stdout, nullptr, _IONBF, 0);

    // A small helper for making a vault from the command line, used to try the
    // program out with a vault that already exists.
    if (argc >= 4 && QString::fromLatin1(argv[1]) == QLatin1String("--make-vault")) {
        mmc::Vault vault;
        QString error;
        if (!vault.create(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]), &error)) {
            std::printf("%s\n", qPrintable(error));
            return 1;
        }
        std::printf("vault written to %s\n", argv[2]);
        return 0;
    }

    QCoreApplication app(argc, argv);

    TestRunner runner;

    runner.begin("crypto");
    test_crypto(runner);

    runner.begin("vault");
    test_vault(runner);

    runner.begin("mime");
    test_mime(runner);

    runner.begin("importer");
    test_importer(runner);

    runner.begin("imap");
    test_imap(runner);

    std::printf("\n%d checks, %d failures\n", runner.checks, runner.failures);
    return runner.failures == 0 ? 0 : 1;
}
