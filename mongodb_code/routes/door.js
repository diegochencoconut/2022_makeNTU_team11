const express = require('express');
const router = express.Router();
const Post = require('../models/Door');
const bodyParser = require('body-parser')
const jsonParser = bodyParser.json()

router.get('/', async (req, res) => {
    try
    {
        const test = await Post.findOne(
                { $or:[
                   {door: "open"},
                   {door: "request"},
                ]}
        )
        if (test == null)
        {
            res.json({"result": "accepted"});
        }
        else
        {
            res.json({"result": "rejected"});
        }
    }
    catch(err)
    {
        res.json({message:err});
    }
});

router.post('/', jsonParser, async (req, res) => {
    const post = new Post({
        room: req.body.room,
        door: req.body.door,
    });
    try
    {
        const test = await Post.findOne(
            {room: req.body.room}
        )

        if (test == null)
        {
            const savedPost = await post.save();
            console.log(savedPost);
            res.json({"message": "The room is created"});
        }
        else
        {
            res.json({"message": "Room existed"})
        }
    }
    catch(err)
    {
        res.json({message: err})
    }
});

//TODO
router.patch('/:room', jsonParser, async(req, res)=>{
    try
    {
        if (req.body.door == "request")
        {
            const test = await Post.findOne(
                { $or:[
                   {door: "open"},
                   {door: "request"},
                ]}
            )
            if (test == null)
            {
                const updatedPost = await Post.updateOne(
                    {
                        type: "Door",
                        room: req.params.room
                    },
                    {$set: {door: "request"}}
                );
                res.json({"message": "accepted"});
            }
            else
            {
                res.json({"message": "rejected"});
            }
        }
        else
        {
            const updatedPost = await Post.updateOne(
                {
                    type: "Door",
                    room: req.params.room
                },
                {$set: {door: req.body.door}}
            );
            res.json({"message": "success"});
        }
    }
    catch(err)
    {
        res.json({message: err})
    }
})


module.exports = router;