define([
    'jquery', 'Backbone', 'app'
], function($, BB, app) {

    var V = BB.View.extend({
        className: 'st',
        mid: 'st',
        name: 'Записи',
        initialize: function() {
            this.$el;
        },
    });

    return (new V);

});
