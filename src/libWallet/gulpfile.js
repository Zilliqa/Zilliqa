var browserify = require('browserify');
var gulp = require('gulp');
var source = require('vinyl-source-stream');
var buffer = require('vinyl-buffer');
var uglify = require('gulp-uglify');
var gutil = require('gulp-util');

gulp.task('build', function () {
  // set up the browserify instance on a task basis
  var b = browserify({
    entries: './index.js',
    debug: true
  });

  return b.bundle()
    .pipe(source('z-lib.min.js'))
    .pipe(buffer())
    // Add transformation tasks to the pipeline here
    .pipe(uglify())
    .on('error', gutil.log)
    .pipe(gulp.dest('./build/'));
});
