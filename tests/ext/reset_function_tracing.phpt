--TEST--
Check a function can be untraced.
--ENV--
DD_TRACE_WARN_LEGACY_DD_TRACE=0
--SKIPIF--
<?php if (PHP_MAJOR_VERSION > 5) die('skip: test requires legacy API'); ?>
--FILE--
<?php

dd_trace("spl_autoload_register", function() {
    echo "HOOK" . PHP_EOL;
    return call_user_func_array('spl_autoload_register', func_get_args());
});

spl_autoload_register(function($class) {
    return false;
});

spl_autoload_register(function($class) {
    return false;
});

dd_untrace("spl_autoload_register");

spl_autoload_register(function($class) {
    return false;
});

// Also testing that if a function does not exists dd_untrace does not throw
// an exception
dd_untrace("this_function_does_not_exist");

?>
--EXPECT--
HOOK
HOOK
