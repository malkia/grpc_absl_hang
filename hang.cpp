#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/provider.h>

#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>

#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>

#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>

using namespace opentelemetry;

static void init_metrics()
{
	exporter::otlp::OtlpGrpcMetricExporterOptions exporterOptions;
	exporterOptions.endpoint = "localhost:65535"; // unlikely to exist, we don't need actual endpoint to replicate the bug
	auto exporter{ exporter::otlp::OtlpGrpcMetricExporterFactory::Create( exporterOptions ) };
	sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
	readerOptions.export_interval_millis = std::chrono::milliseconds(500);
	readerOptions.export_timeout_millis  = std::chrono::milliseconds(250);
	auto reader{ sdk::metrics::PeriodicExportingMetricReaderFactory::Create( std::move( exporter ), readerOptions ) };
	nostd::shared_ptr<sdk::metrics::MeterProvider> meterProvider{ sdk::metrics::MeterProviderFactory::Create() };
	meterProvider->AddMetricReader( std::move( reader ) );
	metrics::Provider::SetMeterProvider( std::move( meterProvider ) );
}

static void init_logs()
{
	exporter::otlp::OtlpGrpcLogRecordExporterOptions exporterOptions;
	exporterOptions.endpoint = "localhost:65535"; // unlikely to exist, we don't need actual endpoint to replicate the bug
	auto exporter = exporter::otlp::OtlpGrpcLogRecordExporterFactory::Create( exporterOptions );
	sdk::logs::BatchLogRecordProcessorOptions processorOptions;
	processorOptions.max_export_batch_size = 8192;
	processorOptions.max_queue_size = 65536;
	processorOptions.schedule_delay_millis = std::chrono::milliseconds{ 10000 };
	auto processor = sdk::logs::BatchLogRecordProcessorFactory::Create( std::move( exporter ), processorOptions );
	std::vector<std::unique_ptr<sdk::logs::LogRecordProcessor>> logRecordProcessors;
	logRecordProcessors.emplace_back( std::move( processor ) );
	nostd::shared_ptr<sdk::logs::LoggerProvider> loggerProvider( sdk::logs::LoggerProviderFactory::Create( std::move( logRecordProcessors ) ) );
	logs::Provider::SetLoggerProvider( std::move( loggerProvider ) );
}

int main(int argc, const char* argv[])
{
  init_metrics();
  init_logs();
  _Exit(0);
}
