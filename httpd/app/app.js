define([
    'Backbone', 'cams'
], function(BB, Cams) {

    var M = BB.Model.extend({
        defaults: {
            authorized: false,
        },
        // initialize: function() {
        // },
    });

    return (new M);

});
