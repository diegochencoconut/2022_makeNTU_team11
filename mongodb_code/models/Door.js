const mongoose = require('mongoose');

const DoorSchema = mongoose.Schema({
    type:
    {
        type: String,
        default: "Door",
    },
    room:
    {
        type: Number,
        required: true,
    },
    door:
    {
        type: String,
        require: true,
        //request, open, close
    },
    date:
    {
        type: Date,
        default: Date.now,
    },

})

module.exports = mongoose.model('Door', DoorSchema);