require.config({
    baseUrl: "/app",
    paths: {
        domReady: "/lib/ext/domready/domReady",
        text: "/lib/ext/text/text",
	crypto_sha1: '/lib/ext/CryptoJS/build/rollups/sha1',
	jquery: '/lib/ext/jquery/dist/jquery.min',
	underscore: '/lib/ext/underscore/underscore',
	Backbone: '/lib/ext/backbone/backbone-min',
	tulip: '/lib/ext/tulip',

        templates: '/templates',
    },
    shim: {
	crypto_sha1: {exports: 'CryptoJS'},
        jquery: {exports: 'jQuery'},
        underscore: {exports: '_'},
    },
});
var log = console.log.bind(console);
require(['domReady!', 'main']);
