// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.jdisc.http.server.jetty;

import com.yahoo.container.logging.ConnectionLogEntry;
import com.yahoo.security.SslContextBuilder;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLHandshakeException;
import java.io.IOException;
import java.net.SocketException;
import java.nio.file.Path;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import static com.yahoo.jdisc.http.server.jetty.Utils.createSslTestDriver;
import static com.yahoo.jdisc.http.server.jetty.Utils.generatePrivateKeyAndCertificate;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assertions.fail;
import static org.junit.jupiter.api.Assumptions.assumeTrue;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.verify;

/**
 * @author bjorncs
 */
class SslHandshakeMetricsIT {

    private static final Logger log = Logger.getLogger(SslHandshakeMetricsIT.class.getName());
    private static Path privateKeyFile;
    private static Path certificateFile;

    @BeforeAll
    static void generateCrypto(@TempDir Path tmpFolder) throws IOException {
        privateKeyFile = tmpFolder.resolve("private-key.pem");
        certificateFile = tmpFolder.resolve("certificate.pem");
        generatePrivateKeyAndCertificate(privateKeyFile, certificateFile);
    }

    @Test
    void requireThatMetricIsIncrementedWhenClientIsMissingCertificateOnHandshake() throws IOException {
        var metricConsumer = new MetricConsumerMock();
        InMemoryConnectionLog connectionLog = new InMemoryConnectionLog();
        JettyTestDriver driver = createSslTestDriver(certificateFile, privateKeyFile, metricConsumer, connectionLog);

        SSLContext clientCtx = new SslContextBuilder()
                .withTrustStore(certificateFile)
                .build();
        assertHttpsRequestTriggersSslHandshakeException(
                driver, clientCtx, null, null, "Received fatal alert: bad_certificate");
        verify(metricConsumer.mockitoMock(), atLeast(1))
                .add(MetricDefinitions.SSL_HANDSHAKE_FAILURE_MISSING_CLIENT_CERT, 1L, MetricConsumerMock.STATIC_CONTEXT);
        assertTrue(driver.close());
        assertEquals(1, connectionLog.logEntries().size());
        assertSslHandshakeFailurePresent(
                connectionLog.logEntries().get(0), SSLHandshakeException.class, SslHandshakeFailure.MISSING_CLIENT_CERT.failureType());
    }

    @Test
    void requireThatMetricIsIncrementedWhenClientUsesIncompatibleTlsVersion() throws IOException {
        var metricConsumer = new MetricConsumerMock();
        InMemoryConnectionLog connectionLog = new InMemoryConnectionLog();
        JettyTestDriver driver = createSslTestDriver(certificateFile, privateKeyFile, metricConsumer, connectionLog);

        SSLContext clientCtx = new SslContextBuilder()
                .withTrustStore(certificateFile)
                .withKeyStore(privateKeyFile, certificateFile)
                .build();

        boolean tlsv11Enabled = List.of(clientCtx.getDefaultSSLParameters().getProtocols()).contains("TLSv1.1");
        assumeTrue(tlsv11Enabled, "TLSv1.1 must be enabled in installed JDK");

        assertHttpsRequestTriggersSslHandshakeException(driver, clientCtx, "TLSv1.1", null, "protocol");
        verify(metricConsumer.mockitoMock(), atLeast(1))
                .add(MetricDefinitions.SSL_HANDSHAKE_FAILURE_INCOMPATIBLE_PROTOCOLS, 1L, MetricConsumerMock.STATIC_CONTEXT);
        assertTrue(driver.close());
        assertEquals(1, connectionLog.logEntries().size());
        assertSslHandshakeFailurePresent(
                connectionLog.logEntries().get(0), SSLHandshakeException.class, SslHandshakeFailure.INCOMPATIBLE_PROTOCOLS.failureType());
    }

    @Test
    void requireThatMetricIsIncrementedWhenClientUsesIncompatibleCiphers() throws IOException {
        var metricConsumer = new MetricConsumerMock();
        InMemoryConnectionLog connectionLog = new InMemoryConnectionLog();
        JettyTestDriver driver = createSslTestDriver(certificateFile, privateKeyFile, metricConsumer, connectionLog);

        SSLContext clientCtx = new SslContextBuilder()
                .withTrustStore(certificateFile)
                .withKeyStore(privateKeyFile, certificateFile)
                .build();

        assertHttpsRequestTriggersSslHandshakeException(
                driver, clientCtx, null, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", "Received fatal alert: handshake_failure");
        verify(metricConsumer.mockitoMock(), atLeast(1))
                .add(MetricDefinitions.SSL_HANDSHAKE_FAILURE_INCOMPATIBLE_CIPHERS, 1L, MetricConsumerMock.STATIC_CONTEXT);
        assertTrue(driver.close());
        assertEquals(1, connectionLog.logEntries().size());
        assertSslHandshakeFailurePresent(
                connectionLog.logEntries().get(0), SSLHandshakeException.class, SslHandshakeFailure.INCOMPATIBLE_CIPHERS.failureType());
    }

    @Test
    void requireThatMetricIsIncrementedWhenClientUsesInvalidCertificateInHandshake(@TempDir Path tmpFolder) throws IOException {
        var metricConsumer = new MetricConsumerMock();
        InMemoryConnectionLog connectionLog = new InMemoryConnectionLog();
        JettyTestDriver driver = createSslTestDriver(certificateFile, privateKeyFile, metricConsumer, connectionLog);

        Path clientPrivateKeyFile = tmpFolder.resolve("client-key.pem");
        Path clientCertificateFile = tmpFolder.resolve("client-cert.pem");
        generatePrivateKeyAndCertificate(clientPrivateKeyFile, clientCertificateFile);

        SSLContext clientCtx = new SslContextBuilder()
                .withKeyStore(clientPrivateKeyFile, clientCertificateFile)
                .withTrustStore(certificateFile)
                .build();

        assertHttpsRequestTriggersSslHandshakeException(
                driver, clientCtx, null, null, "Received fatal alert: certificate_unknown");
        verify(metricConsumer.mockitoMock(), atLeast(1))
                .add(MetricDefinitions.SSL_HANDSHAKE_FAILURE_INVALID_CLIENT_CERT, 1L, MetricConsumerMock.STATIC_CONTEXT);
        assertTrue(driver.close());
        assertEquals(1, connectionLog.logEntries().size());
        assertSslHandshakeFailurePresent(
                connectionLog.logEntries().get(0), SSLHandshakeException.class, SslHandshakeFailure.INVALID_CLIENT_CERT.failureType());
    }

    @Test
    void requireThatMetricIsIncrementedWhenClientUsesExpiredCertificateInHandshake(@TempDir Path tmpFolder) throws IOException {
        Path endEntityKeyFile = tmpFolder.resolve("client-key.pem");
        Path endEntitycertificateFile = tmpFolder.resolve("client-cert.pem");
        Instant notAfter = Instant.now().minus(100, ChronoUnit.DAYS);
        generatePrivateKeyAndCertificate(privateKeyFile, certificateFile, endEntityKeyFile, endEntitycertificateFile, notAfter);
        var metricConsumer = new MetricConsumerMock();
        InMemoryConnectionLog connectionLog = new InMemoryConnectionLog();
        JettyTestDriver driver = createSslTestDriver(certificateFile, privateKeyFile, metricConsumer, connectionLog);

        SSLContext clientCtx = new SslContextBuilder()
                .withTrustStore(certificateFile)
                .withKeyStore(endEntityKeyFile, endEntitycertificateFile)
                .build();

        assertHttpsRequestTriggersSslHandshakeException(
                driver, clientCtx, null, null, "Received fatal alert: certificate_unknown");
        verify(metricConsumer.mockitoMock(), atLeast(1))
                .add(MetricDefinitions.SSL_HANDSHAKE_FAILURE_EXPIRED_CLIENT_CERT, 1L, MetricConsumerMock.STATIC_CONTEXT);
        assertTrue(driver.close());
        assertEquals(1, connectionLog.logEntries().size());
    }


    private static void assertHttpsRequestTriggersSslHandshakeException(
            JettyTestDriver testDriver,
            SSLContext sslContext,
            String protocolOverride,
            String cipherOverride,
            String expectedExceptionSubstring) throws IOException {
        List<String> protocols = protocolOverride != null ? List.of(protocolOverride) : null;
        List<String> ciphers = cipherOverride != null ? List.of(cipherOverride) : null;
        try (var client = new SimpleHttpClient(sslContext, protocols, ciphers, testDriver.server().getListenPort(), false)) {
            client.get("/status.html");
            fail("SSLHandshakeException expected");
        } catch (SSLHandshakeException e) {
            assertTrue(e.getMessage().contains(expectedExceptionSubstring));
        } catch (SocketException | SSLException e) {
            // This exception is thrown if Apache httpclient's write thread detects the handshake failure before the read thread.
            var msg = e.getMessage();
            log.log(Level.WARNING, "Client failed to get a proper TLS handshake response: " + msg, e);
            // Only ignore a subset of exceptions
            assertTrue(msg.contains("readHandshakeRecord") || msg.contains("Broken pipe") || msg.contains("Connection reset"),
                       e.toString());
        }
    }

    private static void assertSslHandshakeFailurePresent(
            ConnectionLogEntry entry, Class<? extends SSLHandshakeException> expectedException, String expectedType) {
        assertTrue(entry.sslHandshakeFailure().isPresent());
        ConnectionLogEntry.SslHandshakeFailure failure = entry.sslHandshakeFailure().get();
        assertEquals(expectedType, failure.type());
        ConnectionLogEntry.SslHandshakeFailure.ExceptionEntry exceptionEntry = failure.exceptionChain().get(0);
        assertEquals(expectedException.getName(), exceptionEntry.name());
    }

}
