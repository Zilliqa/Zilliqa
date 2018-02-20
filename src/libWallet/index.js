var zLib = require('./lib/zLib')

if (typeof window !== 'undefined' && typeof window.zLib === 'undefined') {
  window.zLib = zLib
}

module.exports = {
	zLib: zLib
}
