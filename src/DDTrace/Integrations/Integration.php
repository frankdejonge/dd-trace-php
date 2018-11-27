<?php

namespace DDTrace\Integrations;

use DDTrace\Tags;
use OpenTracing\Span;
use OpenTracing\GlobalTracer;

abstract class Integration
{
    const CLASS_NAME = '';

    public static function load()
    {
        if (!extension_loaded('ddtrace')) {
            trigger_error('The ddtrace extension is required to trace ' . static::CLASS_NAME, E_USER_WARNING);
            return;
        }
        if (!class_exists(static::CLASS_NAME)) {
            trigger_error(static::CLASS_NAME . ' is not loaded and cannot be traced', E_USER_WARNING);
            return;
        }
    }

    /**
     * @param string $method
     * @param \Closure $spanMutator
     */
    protected static function traceMethod($method, \Closure $spanMutator = null)
    {
        $className = static::CLASS_NAME;
        $integrationClass = get_called_class();
        dd_trace($className, $method, function () use ($className, $integrationClass, $method, $spanMutator) {
            $args = func_get_args();
            $scope = GlobalTracer::get()->startActiveSpan($className . '.' . $method);
            $span = $scope->getSpan();
            $integrationClass::setDefaultTags($span, $method);
            if (null !== $spanMutator) {
                $spanMutator($span, $args);
            }

            $returnVal = null;
            $thrownException = null;
            try {
                $returnVal = call_user_func_array([$this, $method], $args);
            } catch (\Exception $e) {
                $span->setError($e);
                $thrownException = $e;
            }
            $scope->close();
            if (null !== $thrownException) {
                throw $thrownException;
            }
            return $returnVal;
        });
    }

    public static function setDefaultTags(Span $span, $method)
    {
        $span->setTag(Tags\RESOURCE_NAME, $method);
    }
}
