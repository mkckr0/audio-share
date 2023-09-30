# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

# Uncomment this to preserve the line number information for
# debugging stack traces.
#-keepattributes SourceFile,LineNumberTable

# If you keep the line number information, uncomment this to
# hide the original source file name.
#-renamesourcefileattribute SourceFile

-dontobfuscate

-keep class io.github.mkckr0.audio_share_app.pb.** { *; }

-assumenosideeffects class android.util.Log {
    public static *** v(...);
    public static *** d(...);
}

-keep class io.netty.channel.socket.nio.* { *; }
-keep class io.netty.handler.codec.* { *; }
-keep class io.netty.util.* { *; }

-dontwarn com.oracle.svm.core.annotate.*
-dontwarn org.apache.log4j.*
-dontwarn org.apache.logging.log4j.**
-dontwarn org.jetbrains.annotations.*
-dontwarn org.slf4j.**
-dontwarn reactor.blockhound.integration.*